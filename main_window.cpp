#include "main_window.h"
#include "ui_main_window.h"
#include <QSerialPort>
#include <qcustomplot.h>
#include <QDebug>
#include <QtMath>
#include <QDateTime>
#include <QDir>
#include <QKeyEvent>
#include <QFileInfo>
#include <QPixmap>

main_window::main_window(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::main_window)
{
    ui->setupUi(this);

    initializeMainWindow();

    hcom1 = new QSerialPort();
}

main_window::~main_window()
{
    delete hcom1;
    delete ui;
}

void main_window::keyPressEvent(QKeyEvent *event)
{
    QString cmd;
    switch (event->key()) {
    case Qt::Key_W:
        cmd = "UP";
        // sendCommand("UP");
        break;
    case Qt::Key_S:
        cmd = "DOWN";
        // sendCommand("DOWN");
        break;
    case Qt::Key_A:
        cmd = "BACK";
        // sendCommand("BACK");
        break;
    case Qt::Key_D:
        cmd = "FORW";
        // sendCommand("FORW");
        break;
    case Qt::Key_O:
        cmd = "OK";
        // sendCommand("OK");
        break;
    default:
        QMainWindow::keyPressEvent(event); // pass on unhandled keys
    }

    if(!cmd.isEmpty())
    {
        statusBar()->showMessage("Sent " + cmd + " cmd", 1000);
        hcom1->write((cmd+"\n").toUtf8());
    }
}

void main_window::on_popen_button_clicked()
{
    UartConfig config = getUartConfigFromUI();

    hcom1->setPortName(config.portName);
    hcom1->setBaudRate(config.baudRate);
    hcom1->setDataBits(config.dataBits);
    hcom1->setParity(config.parity);
    hcom1->setStopBits(config.stopBits);
    hcom1->setFlowControl(config.flowControl);

    qDebug() << "UART config applied with port:" << config.portName;

    if (hcom1->open(QIODevice::ReadWrite))
    {
        port_options(DISABLE);
        connect(hcom1, SIGNAL(readyRead()), this, SLOT(uartRead()));
        // qDebug() << "Serial port opened successfully.";
        statusBar()->showMessage("Serial port " + config.portName +" opened successfully.", 1000);
    }
    else
    {
        // qDebug() << "Failed to open serial port:" << hcom1->errorString();
        statusBar()->showMessage("Failed to open serial port: " + hcom1->errorString(), 1000);
    }
}

void main_window::on_pclose_button_clicked()
{
    if (hcom1->isOpen())
    {
        // Clear all displayed data
        if (ui->plot && ui->plot->graphCount() > 0 && ui->plot->graph(0)) {
            ui->plot->graph(0)->data()->clear();
            ui->plot->graph(1)->data()->clear();
            ui->plot->replot();
        }

        // Double check in case previous logFie still opens
        // -> close it before closing the Port
        if (isLog)  // In case no logFile opened
        {
            logFile.close();
            outLogFile.setDevice(nullptr);
        }

        // Reset all state, index variables
        port_options(ENABLE);
        timeIndex = 0;
        isReady = false;
        isPause = false;
        isLog = false;

        hcom1->close();

        // qDebug() << "Serial port closed.";
        statusBar()->showMessage("Serial port " + hcom1->portName() + " closed.", 1000);
    }
    else
    {
        // qDebug() << "Serial port was already closed.";
        statusBar()->showMessage("Serial port " + hcom1->portName() + " was already closed.", 1000);
    }   
}

void main_window::on_send_button_clicked()
{
    if (hcom1->isOpen())
    {
        freqSettings.startFreqHz = convertToHz(ui->fstart_data->text().toDouble(), ui->fstart_unit->currentText());
        freqSettings.stopFreqHz  = convertToHz(ui->fend_data->text().toDouble(), ui->fend_unit->currentText());
        freqSettings.stepFreqHz  = convertToHz(ui->fstep_data->text().toDouble(), ui->fstep_unit->currentText());
        freqSettings.rfPout      = ui->rf_pout_data->text().toDouble(); // assuming dBm, no conversion

        freqSettings.sampleCount = (freqSettings.stopFreqHz - freqSettings.startFreqHz) / freqSettings.stepFreqHz + 1;
        freqSettings.currFreq = freqSettings.startFreqHz;

        freqSettings.max.freqVal = 4400000000;
        freqSettings.max.adcVal = 00000.0;
        freqSettings.min.freqVal = 0000000000;
        freqSettings.min.adcVal = 10000.0;

        // Only send commands (header) -> MCU if checked
        // otherwise the freqSettings are configured independently by a 3rd-party
        uartWrite();
    }
}

void main_window::on_pause_button_clicked()
{
    isPause = !isPause; // Toggle pause state
}

void main_window::on_run_button_clicked()
{
    if (hcom1->isOpen())
    {
        freqSettings.startFreqHz = convertToHz(ui->fstart_data->text().toDouble(), ui->fstart_unit->currentText());
        freqSettings.stopFreqHz  = convertToHz(ui->fend_data->text().toDouble(), ui->fend_unit->currentText());
        freqSettings.stepFreqHz  = convertToHz(ui->fstep_data->text().toDouble(), ui->fstep_unit->currentText());
        freqSettings.rfPout      = ui->rf_pout_data->text().toDouble(); // assuming dBm, no conversion

        freqSettings.sampleCount = (freqSettings.stopFreqHz - freqSettings.startFreqHz) / freqSettings.stepFreqHz + 1;
        freqSettings.currFreq = freqSettings.startFreqHz;

        freqSettings.max.freqVal = 4400000000;
        freqSettings.max.adcVal = -100.0;
        freqSettings.min.freqVal = 0000000000;
        freqSettings.min.adcVal = 100.0;

        freq_options(DISABLE);
        // scalingSelection(ENABLE);

        if( freqSettings.startFreqHz > 0 &&
            freqSettings.stepFreqHz > 0 &&
            freqSettings.stopFreqHz > 0 &&
            freqSettings.sampleCount > 0)
        {
            isReady = true;
            isPause = false;

            initializePlot();

            ui->plot->graph(0)->data()->clear();
            ui->plot->graph(1)->data()->clear();
            ui->plot->replot();

            // Double check in case previous logFie still opens
            // -> close it before creating new one
            if (isLog)
            {
                logFile.close();    //Close the old file
                outLogFile.setDevice(nullptr);
                // qDebug() << "Previous log file closed.!";
            }
            // Enable Log feature when its box is checked!
            if(ui->outDir_info->isChecked()) isLog = true;

            if (ui->mode_data->currentText() == "RF Scanning") initializeRFScanMode();
            else if (ui->mode_data->currentText() == "S11 Plot") initializeS11PlotMode();
            else qWarning() << "No supported mode is selected" << ui->mode_data;

            statusBar()->showMessage("Waiting measurement data!", 1000);
        }
    }

}

void main_window::on_stop_button_clicked()
{
    // Clear all displayed data
    ui->plot->graph(0)->data()->clear();
    ui->plot->graph(1)->data()->clear();
    ui->plot->replot();

    // Close and clear logFile
    if (isLog)  // In case no logFile opened
    {
        logFile.close();
        outLogFile.setDevice(nullptr);
    }

    // Reset all state, index variables
    timeIndex = 0;
    isReady = false;
    isPause = false;
    isLog = false;

    freq_options(ENABLE);
    // scalingSelection(DISABLE);
    hcom1->write("Stop\n");
    statusBar()->showMessage("Stop measurement!", 1000);
}
\
void main_window::on_actionExit_triggered()
{
    if(hcom1->isOpen()) hcom1->close();

    QApplication::quit();
}

void main_window::on_yscale_data_currentTextChanged(const QString &arg1)
{
    if(!isReady)    // Only modify when stopped
    {
        bool ok = false;
        scaledFactor = arg1.toUInt(&ok);

        qDebug() << "New scaling factor:" << scaledFactor;

        // Clear all current data
        // ui->plot->graph(0)->data()->clear();
    }
}

void main_window::initializeMainWindow()
{
    menuBar()->setStyleSheet
        (R"(
        QMenuBar { color: white; background-color: #2a2a2a; }
        QMenu { color: white; background-color: #2a2a2a; }
    )");
    // menuBar()->setStyleSheet("QMenuBar { color: white; background-color: #2a2a2a; }");
    // menuBar()->setStyleSheet("QMenu { color: white; background-color: #2a2a2a; }");
    statusBar()->setStyleSheet("QStatusBar { color: white; background-color: #2a2a2a; }");

    // scalingSelection(DISABLE);
}

void main_window::initializePlot()
{
    ui->plot->addGraph();
    ui->plot->graph(0)->setPen(QPen(Qt::blue));
    ui->plot->graph(0)->setScatterStyle(QCPScatterStyle::ssNone);
    ui->plot->graph(0)->setLineStyle(QCPGraph::lsLine);

    ui->plot->addGraph();
    ui->plot->graph(1)->setPen(QPen(Qt::red));
    ui->plot->graph(1)->setScatterStyle(QCPScatterStyle::ssNone);
    ui->plot->graph(1)->setLineStyle(QCPGraph::lsLine);
}

void main_window::initializeRFScanMode()
{
    if(isLog)
    {
        // Create new file with current timestamp
        QString filename = logPath + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + "_rfScanning.csv";
        logFile.setFileName(filename);
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Failed to open log file:" << filename;
        }
        outLogFile.setDevice(&logFile);
    }

    ui->plot->graph(0)->setName("RF Scanning");
    ui->plot->graph(0)->setValueAxis(ui->plot->yAxis);
    ui->plot->xAxis->setLabel("Frequency (Hz)");
    ui->plot->yAxis->setLabel("Magnitudes (dB)");
    ui->plot->xAxis->setRange(freqSettings.startFreqHz, freqSettings.stopFreqHz*1.05);
    ui->plot->yAxis->setRange(-40.0, 5.0);     // in Unit: dB

    ui->plot->graph(1)->setName("SRR Reflection");
    ui->plot->graph(1)->setValueAxis(ui->plot->yAxis2);
    ui->plot->yAxis2->setLabel("Phases (deg)");
    ui->plot->yAxis2->setRange(-180.0, 180.0);
    ui->plot->yAxis2->setVisible(true);

    // ui->plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    // QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
    // ui->plot->yAxis->setTicker(logTicker);
}

void main_window::initializeS11PlotMode()
{
    if(isLog)
    {
        // Create new file with current timestamp
        QString filename = logPath + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + "_S11Plot.csv";
        logFile.setFileName(filename);
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Failed to open log file:" << filename;
        }
    }

    outLogFile.setDevice(&logFile);
    ui->plot->graph(0)->setName("S11 Plot");
    ui->plot->xAxis->setLabel("Time (sec)");
    ui->plot->yAxis->setLabel("ADC Value");
    ui->plot->xAxis->setRange(0.0, 10.0);
    ui->plot->yAxis->setRange(50.0, 80.0);     // in Unit: dB
}

void main_window::updateXAxisRange(double windowSize)
{
    double xMin = std::max(0.0, timeVal - windowSize);
    double xMax = timeVal;
    ui->plot->xAxis->setRange(xMin, xMax);

    if (timeIndex == 10000)
    {
        timeIndex = 0;
        ui->plot->graph(0)->data()->clear();
    }
}

void main_window::updateYAxisRange(double data, double offset)
{
    double yMin = std::max(0.0, data - offset);
    double yMax = data + offset;
    ui->plot->yAxis->setRange(yMin, yMax);
}

void main_window::port_options(bool option)
{
    ui->port_data->setEnabled(option);
    ui->brate_data->setEnabled(option);
    ui->parity_data->setEnabled(option);
    ui->dbits_data->setEnabled(option);
    ui->sbits_data->setEnabled(option);
    ui->fctrl_data->setEnabled(option);
}

void main_window::freq_options(bool option)
{
    ui->fstart_data->setEnabled(option);
    ui->fend_data->setEnabled(option);
    ui->fstep_data->setEnabled(option);
    ui->rf_pout_data->setEnabled(option);
    ui->mode_data->setEnabled(option);
}

void main_window::scalingSelection(bool option)
{
    ui->yscale_data->setEnabled(option);
}

double main_window::scalingYAxisVal(double data)
{
    double scaledVal = scaledFactor * data;
    // updateYAxisRange(scaledVal, 1500);
    return scaledVal;
}

UartConfig main_window::getUartConfigFromUI()
{
    UartConfig config;

    config.portName = ui->port_data->text();  // Assuming QLineEdit for port name

    int baud = ui->brate_data->currentText().toInt();
    config.baudRate = static_cast<QSerialPort::BaudRate>(baud);

    int dbits = ui->dbits_data->currentText().toInt();
    config.dataBits = static_cast<QSerialPort::DataBits>(dbits);

    QString parityText = ui->parity_data->currentText().toLower();
    if (parityText == "none")       config.parity = QSerialPort::NoParity;
    else if (parityText == "even")  config.parity = QSerialPort::EvenParity;
    else if (parityText == "odd")   config.parity = QSerialPort::OddParity;
    else if (parityText == "mark")  config.parity = QSerialPort::MarkParity;
    else if (parityText == "space") config.parity = QSerialPort::SpaceParity;

    QString sbits = ui->sbits_data->currentText();
    if (sbits == "1")       config.stopBits = QSerialPort::OneStop;
    else if (sbits == "1.5") config.stopBits = QSerialPort::OneAndHalfStop;
    else if (sbits == "2")   config.stopBits = QSerialPort::TwoStop;

    QString flowText = ui->fctrl_data->currentText().toLower();
    if (flowText == "none")       config.flowControl = QSerialPort::NoFlowControl;
    else if (flowText == "rts/cts") config.flowControl = QSerialPort::HardwareControl;
    else if (flowText == "xon/xoff") config.flowControl = QSerialPort::SoftwareControl;

    return config;
}

void main_window::uartWrite()
{
    // Split each frequency
    uint8_t firstBitStart = freqSettings.startFreqHz / 1'000'000'000;
    uint32_t otherBitsStart = freqSettings.startFreqHz % 1'000'000'000;
    uint8_t firstBitStop = freqSettings.stopFreqHz / 1'000'000'000;
    uint32_t otherBitsStop = freqSettings.stopFreqHz % 1'000'000'000;

    qDebug() << QString("[FirstBitStart] = 0x%1").arg(firstBitStart, 2, 16, QChar('0')).toUpper();
    qDebug() << QString("[OtherBitsStart]= 0x%1").arg(otherBitsStart, 8, 16, QChar('0')).toUpper();
    qDebug() << QString("[FirstBitStop]  = 0x%1").arg(firstBitStop, 2, 16, QChar('0')).toUpper();
    qDebug() << QString("[OtherBitsStop] = 0x%1").arg(otherBitsStop, 8, 16, QChar('0')).toUpper();
    qDebug() << QString("[OtherBitsStop] = 0x%1").arg(freqSettings.stepFreqHz, 8, 16, QChar('0')).toUpper();

    // Format header string to match STM32
    QString header = QString("%1%2%3%4%5\n")
                         .arg(firstBitStart, 2, 16, QChar('0'))
                         .arg(otherBitsStart, 8, 16, QChar('0'))
                         .arg(firstBitStop, 2, 16, QChar('0'))
                         .arg(otherBitsStop, 8, 16, QChar('0'))
                         .arg(freqSettings.stepFreqHz, 8, 16, QChar('0'))
                         .toUpper();

    hcom1->write("Start\n");
    qDebug() << "Start command sent!";
    statusBar()->showMessage("Start command sent!", 1000);

    QTimer::singleShot(100, this, [=]() {
        hcom1->write(header.toUtf8());
        statusBar()->showMessage("Header sent!", 1000);
    });
}

void main_window::uartRead()
{
    if(hcom1->isOpen())
    {
        rxData += hcom1->readAll();

        while (rxData.contains('\n'))
        {
            int newLineIndex = rxData.indexOf('\n');
            QByteArray packet = rxData.left(newLineIndex).trimmed();
            // qDebug() << packet;

            if(isReady and !isPause)
            {
                QString mode = ui->mode_data->currentText();
                if (mode == "RF Scanning") rfScan(&packet);
                else if (mode == "S11 Plot") s11Plot(&packet);
                else qDebug() << "Unknown mode:" << mode;
            }

            // Clear buffer up to and including '\n'
            // Clear buffer early to prevent stuck loops
            rxData.remove(0, newLineIndex+1);
        }
    }
}

void main_window::rfScan(QByteArray *packet)
{
    if (packet->size() == 8 and freqSettings.sampleCount >= 0)
    {
        qDebug() << "----------------------";
        bool ok;

        uint16_t adcVal = packet->mid(0, 4).toUShort(&ok, 16);
        uint16_t phsVal = packet->mid(4, 4).toUShort(&ok, 16);
        // Convert to Vmag
        double Vmag = (adcVal/4095.0)*3.0;
        double VmagCP = ui->vmag_cp_data->text().toDouble();
        double Vphs = (phsVal/4095.0)*3.0;
        double VphsCP = ui->vphs_cp_data->text().toDouble();
        // Convert to Vmag(dB)
        double Vmag_dB;
        // if(std::abs(Vmag-1.05) <= 0.1) Vmag_dB = 0.0;
        // else Vmag_dB = (Vmag-1.05)/0.03 - 2.5;
        Vmag_dB = (Vmag-VmagCP)/0.03;

        double Vphs_deg = (VphsCP-Vphs)/0.01;
        // if(Vmag_dB < 0) Vphs_deg = Vphs_deg*(-1.0);

        double Ratio_lin = qPow(10.0, Vmag_dB/20.0);

        //Calculation reflection coefficient
        double R_dB;
        double R = reflectionCalculation(freqSettings.currFreq, Ratio_lin);
        // Convert R to dB
        R_dB = 20.0*log10(R);

        if(R_dB > 0) R_dB = 0;

        //Store max and min data
        if (Vmag_dB < freqSettings.min.adcVal)
        {
            freqSettings.min.adcVal = Vmag_dB;
            freqSettings.min.freqVal = freqSettings.currFreq;
        }
        else if (Vmag_dB > freqSettings.max.adcVal)
        {
            freqSettings.max.adcVal = Vmag_dB;
            freqSettings.max.freqVal = freqSettings.currFreq;
        }

        qDebug() << freqSettings.sampleCount
                 << " | Vmag(adc)=" << adcVal
                 << " | Vphs(adc)=" << phsVal
                 << " | Vmag(V)=" << Vmag
                 << " | Vmag(dB)=" << Vmag_dB
                 << " | Vphs(V)=" << Vphs
                 << " | Vphs(deg)=" << Vphs_deg
                 << " | At_freq=" << freqSettings.currFreq;

        // Add data point
        ui->plot->graph(0)->addData(freqSettings.currFreq, scalingYAxisVal(Vmag_dB));
        ui->plot->graph(1)->addData(freqSettings.currFreq, Vphs_deg);
        ui->plot->replot();

        // Save data logging
        if(isLog)
        {
            outLogFile << freqSettings.currFreq << "," << scalingYAxisVal(adcVal) << "\n";
        }

        freqSettings.currFreq += freqSettings.stepFreqHz;
        freqSettings.sampleCount--;

    }
    else
    {
        qWarning() << "Invalid packet length (" << packet->size() << "):" << packet;

        // Recovery: Drop packet or look for next valid structure
        if (packet->size() > 50) {
            rxData.clear(); // avoid growing garbage
            qWarning() << "Buffer cleared due to overflow.";
        }
    }
}

void main_window::s11Plot(QByteArray *packet)
{
    if (packet->size() == 4)
    {
        bool ok;
        double adcVal = static_cast<double>(packet->toUInt(&ok, 16));

        // Convert to dB
        adcVal = 20*log10(adcVal);

        //Store max and min data
        if (adcVal < freqSettings.min.adcVal)
        {
            freqSettings.min.adcVal = adcVal;
            freqSettings.min.freqVal = freqSettings.currFreq;
        }
        else if (adcVal > freqSettings.max.adcVal)
        {
            freqSettings.max.adcVal = adcVal;
            freqSettings.max.freqVal = freqSettings.currFreq;
        }

        timeVal = timeIndex * 0.01; // Assume that data is sent every 100ms
        qDebug() << freqSettings.sampleCount << "adcVal (dB) =" << adcVal << "at time =" << timeVal;

        // Add data point
        ui->plot->graph(0)->addData(timeVal, scalingYAxisVal(adcVal));
        updateXAxisRange(1);
        ui->plot->replot();

        // Save data logging
        if(isLog)
        {
            outLogFile << timeVal << "," << scalingYAxisVal(adcVal) << "\n";
        }

        timeIndex++;
    }
    else
    {
        qWarning() << "Invalid packet length (" << packet->size() << "):" << packet;

        // Recovery: Drop packet or look for next valid structure
        if (packet->size() > 50) {
            rxData.clear(); // avoid growing garbage
            qWarning() << "Buffer cleared due to overflow.";
        }
    }
}

uint64_t main_window::convertToHz(double value, const QString &unit)
{
    QString u = unit.trimmed().toLower();
    if (u == "ghz") return static_cast<uint64_t>(value * 1e9);
    if (u == "mhz") return static_cast<uint64_t>(value * 1e6);
    if (u == "khz") return static_cast<uint64_t>(value * 1e3);
    return static_cast<uint64_t>(value); // assume already in Hz
}



void main_window::on_outDir_data_textChanged(const QString &arg1)
{
    QDir dir(arg1);
    if (!dir.exists())
    {
        qDebug() << "Warning: Output directory does not exist!";
        // // Create it!!
        // dir.mkdir(".");
    }
    else logPath = arg1 + "/";
}


void main_window::on_max_button_clicked()
{
   // qDebug() << "Max Adc at freq" << freqSettings.max.freqVal;
   // statusBar()->showMessage("Max Adc at freq" + QString::number(freqSettings.max.freqVal), 3000);
    ui->max_data->setText(QString::number(freqSettings.max.freqVal) + " Hz");
}


void main_window::on_min_button_clicked()
{
    // qDebug() << "Min Adc at freq" << freqSettings.min.freqVal;
    // statusBar()->showMessage("Min Adc at freq" + QString::number(freqSettings.min.freqVal), 3000);
    ui->min_data->setText(QString::number(freqSettings.min.freqVal) + " Hz");
}

void main_window::on_sParam_data_editingFinished()
{
    if(ui->sParam_info->isChecked())
    {
        QString type = ui->sParam_type->currentText().trimmed();
        QString filePath = ui->sParam_data->text();
        if(type == "DirectionalCoupler")
        {            
            touchStoneParsing(filePath);
            statusBar()->showMessage("Directional Coupler Type is selected!", 1500);
        }
        else if(type == "None") statusBar()->showMessage("Please select S-Param type!", 1500);
        else; // Do Nothing
    }
    else; // Do Nothing
}

void main_window::touchStoneParsing(const QString& filePath)
{
    // Check the validity of the file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Cannot open file:" << filePath;
        return;
    }

    // Parsing the file (ignoring the header (starting with # or ! characters))
    QTextStream in(&file);
    QVector<QVector<double>> dataRows;
    while (!in.atEnd())
    {
        // Ignore all redundancy whitespaces
        QString line = in.readLine().trimmed();

        // Skip header lines that start with '#' or '!'
        if (line.startsWith("#") || line.startsWith("!")) {
            continue;
        }

        // Split and convert line into numeric values
        static const QRegularExpression regex("\\s+");
        QStringList tokens = line.split(regex, Qt::SkipEmptyParts);
        QVector<double> row;
        for (int i = 0; i < tokens.size(); ++i)
        {
            const QString& token = tokens[i];
            bool ok = false;
            double value = token.toDouble(&ok);
            if (ok) {
                row.append(value);
            } else {
                qWarning() << "Failed to convert:" << token;
            }
        }
        if (!row.isEmpty()) {
            dataRows.append(row);
        }
    }

    // Closing the file
    file.close();

    /* DEBUGGING */
    // for (int i = 0; i < dataRows.size(); ++i) {
    //     const QVector<double>& row = dataRows[i];
    //     QStringList formattedValues;

    //     for (double value : row) {
    //         formattedValues << QString::number(value, 'f', 9); // Adjust precision if needed
    //     }

    //     qDebug() << "Row" << i << ":" << formattedValues.join(", ");
    // }

    // Extract filename from its path
    QString fileName = QFileInfo(filePath).fileName();

    // Identify the corresponding S-param to store data
    std::map<uint64_t, SParameters>* targetMap = nullptr;

    if (fileName.contains("S12", Qt::CaseInsensitive))
    {
        targetMap = &dCoupler.S12;
        ui->s12_info->setStyleSheet("QLabel { color: rgb(170, 255, 0); }");
        // statusBar()->showMessage("Data for Port 1 & 2", 1500);
        // qDebug() << "S12";
    }
    else if (fileName.contains("S13", Qt::CaseInsensitive))
    {
        targetMap = &dCoupler.S13;
        ui->s13_info->setStyleSheet("QLabel { color: rgb(170, 255, 0); }");
        // statusBar()->showMessage("Data for Port 1 & 2", 1500);
        // qDebug() << "S13";
    }
    else if (fileName.contains("S14", Qt::CaseInsensitive))
    {
        targetMap = &dCoupler.S14;
        ui->s14_info->setStyleSheet("QLabel { color: rgb(170, 255, 0); }");
        // statusBar()->showMessage("Data for Port 1 & 2", 1500);
        // qDebug() << "S14";
    }
    else
    {
        qWarning() << "Unknown S-parameter type in file:" << fileName;
        qDebug() << "UNKNOWN";
        return;
    }
    // Clear the old S-params
    if(targetMap) targetMap->clear();

    // Store S-Matrix to corresponding locations
    for (const QVector<double>& row : dataRows) {
        if (row.size() < 9) {
            qWarning() << "Incomplete data row, skipping:" << row;
            continue;
        }

        SParameters newParam;
        // newParam.freq = static_cast<uint64_t>(row[0]);
        newParam.data[0][0] = std::complex<double>(row[1], row[2]); // S11
        newParam.data[0][1] = std::complex<double>(row[3], row[4]); // S21
        newParam.data[1][0] = std::complex<double>(row[5], row[6]); // S12
        newParam.data[1][1] = std::complex<double>(row[7], row[8]); // S22

        // if(targetMap) targetMap->push_back(newParam);
        if(targetMap) (*targetMap)[static_cast<uint64_t>(row[0])] = newParam;
        else qWarning() << "Invalid targetMap";
    }

    // Store the start and stop frequency
    if(!targetMap->empty())
    {
        dCoupler.startFreqHz = targetMap->begin()->first;
        dCoupler.stopFreqHz = std::prev(targetMap->end())->first;

        // Update the GUI freq data
        ui->sParam_start_data->setText(QString::number(dCoupler.startFreqHz) + " Hz");
        ui->sParam_stop_data->setText(QString::number(dCoupler.stopFreqHz) + " Hz");
    }

    /* DEBUGGING */
    // qDebug() << "StartFreq" << dCoupler.startFreqHz;
    // qDebug() << "StopFreq" << dCoupler.stopFreqHz;
    // for (const auto& [freq, param] : *targetMap) {
    //     qDebug() << "Freq:" << freq << "Hz";

    //     qDebug() << "S11:" << param.data[0][0].real() << "+" << param.data[0][0].imag() << "j";
    //     qDebug() << "S12:" << param.data[1][0].real() << "+" << param.data[1][0].imag() << "j";
    //     qDebug() << "S21:" << param.data[0][1].real() << "+" << param.data[0][1].imag() << "j";
    //     qDebug() << "S22:" << param.data[1][1].real() << "+" << param.data[1][1].imag() << "j";

    //     qDebug() << "-------------";
    // }
}

void main_window::on_sParam_type_currentTextChanged(const QString &arg1)
{
    // Update the GUI freq data to default
    ui->sParam_start_data->setText(" ");
    ui->sParam_stop_data->setText(" ");

    // Update respective formula for reflection coefficient (Default)
    ui->reflect_info->clear();
    // Update S-param status to default color
    ui->s12_info->setStyleSheet("QLabel { color: rgb(85, 85, 127); }");
    ui->s13_info->setStyleSheet("QLabel { color: rgb(85, 85, 127); }");
    ui->s14_info->setStyleSheet("QLabel { color: rgb(85, 85, 127); }");

    if(arg1.trimmed() == "DirectionalCoupler")
    {
        // Update respective formula for reflection coefficient
        QPixmap pic("C:/Users/ADM/Desktop/Test/pic1.png");
        ui->reflect_info->setPixmap(pic);
        ui->reflect_info->setPixmap(pic.scaled(ui->reflect_info->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        // Check whether its S-Matrices having data or not
        if(!dCoupler.S12.empty()) ui->s12_info->setStyleSheet("QLabel { color: rgb(170, 255, 0); }");
        if(!dCoupler.S13.empty()) ui->s13_info->setStyleSheet("QLabel { color: rgb(170, 255, 0); }");
        if(!dCoupler.S14.empty()) ui->s14_info->setStyleSheet("QLabel { color: rgb(170, 255, 0); }");
    }
}

double main_window::reflectionCalculation(uint64_t freq, double Vmag_lin)
{
    // Check sParam type
    QString type = ui->sParam_type->currentText().trimmed();
    if(type == "DirectionalCoupler")
    {
        // Ensure S-matrices are not empty
        if((!dCoupler.S12.empty()) && (!dCoupler.S13.empty()) && (!dCoupler.S14.empty()))
        {
            std::complex<double> s12, s21, s31;
            std::complex<double> R, rR;
            // double R, rR;

            // Find the desired freq or the one just behind it
            std::map<uint64_t, SParameters>::iterator it, start, end;

            it = dCoupler.S12.lower_bound(freq);
            // Check the found freq is diffrent to desired freq -> find averaging S-param value
            if(it != dCoupler.S12.begin() && it != dCoupler.S12.end())
            {
                // qDebug() << "------S12-Matrix------";
                // qDebug() << "Freq:" << it->first;
                // Averging the data by its previous and next freq
                if(it->first != freq)
                {
                    end = it;
                    start = --it;
                    double percent = static_cast<double>(freq - start->first) / (end->first - start->first);
                    // qDebug() << "Percentage =" << percent;
                    s12 = start->second.data[1][0] + percent * (end->second.data[1][0] - start->second.data[1][0]);
                    s21 = start->second.data[0][1] + percent * (end->second.data[0][1] - start->second.data[0][1]);
                }
                // Store s12 & s21 values directly
                else
                {
                    s12 = it->second.data[1][0];
                    s21 = it->second.data[0][1];
                }
                // qDebug() << "------------------";
            }
            else qWarning() << "frequency for S12 is out of range!!";


            it = dCoupler.S13.lower_bound(freq);
            // Check the found freq is diffrent to desired freq -> find averaging S-param value
            if(it != dCoupler.S13.begin() && it != dCoupler.S13.end())
            {
                // qDebug() << "------S13-Matrix------";
                // qDebug() << "Freq:" << it->first;
                // Averging the data by its previous and next freq
                if(it->first != freq)
                {
                    end = it;
                    start = --it;
                    double percent = static_cast<double>(freq - start->first) / (end->first - start->first);
                    // qDebug() << "Percentage =" << percent;
                    s31 = start->second.data[0][1] + percent * (end->second.data[0][1] - start->second.data[0][1]);
                }
                // Store s31 value directly
                else s31 = it->second.data[0][1];
                // qDebug() << "------------------";
            }
            else qWarning() << "frequency for S13 is out of range!!";

            // Calculate reflection coefficient (R = 1 // (Vmag*S31*S12 - S12*S21) )
            /* DEBUGGING */
            // qDebug() << "s31 =" << s31.real() << "+" << s31.imag() << "i => Mag =" << std::abs(s31);
            // qDebug() << "s21 =" << s21.real() << "+" << s21.imag() << "i => Mag =" << std::abs(s21);
            // qDebug() << "s12 =" << s12.real() << "+" << s12.imag() << "i => Mag =" << std::abs(s12);
            rR = (1.0/Vmag_lin)*s31*s12 - s12*s21; // R inverse
            // rR = (1.0/Vmag_lin)*std::abs(s31)*std::abs(s12)-std::abs(s12)*std::abs(s21);
            R = 1.0/rR;

            qDebug() << "R =" << std::abs(R);
            return std::abs(R);
        }
    }
    return 1; // Do Nothing
}

