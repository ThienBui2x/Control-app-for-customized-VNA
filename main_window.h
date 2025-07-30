#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QFile>
#include <QTextStream>
#include <complex.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class main_window;
}
QT_END_NAMESPACE

#define ENABLE  true
#define DISABLE false

struct DataPoint
{
    uint64_t freqVal = 0;
    double adcVal = 0.0;
};

struct FrequencySettings
{
    uint64_t startFreqHz = 0;
    uint64_t stopFreqHz = 0;
    uint32_t stepFreqHz = 0;
    uint32_t sampleCount = 0;
    uint64_t currFreq = 0;
    DataPoint max;
    DataPoint min;
    uint8_t rfPout = 0;
};

struct UartConfig
{
    QString portName;
    QSerialPort::BaudRate baudRate;
    QSerialPort::DataBits dataBits;
    QSerialPort::Parity parity;
    QSerialPort::StopBits stopBits;
    QSerialPort::FlowControl flowControl;
};

struct SParameters
{
    uint64_t freq;
    std::complex<double> data[2][2];

};

struct DirectionalCoupler
{
    uint64_t startFreqHz = 0;
    uint64_t stopFreqHz = 0;
    std::vector<SParameters> S12;
    std::vector<SParameters> S13;
    std::vector<SParameters> S14;
};

class main_window : public QMainWindow
{
    Q_OBJECT

public:
    main_window(QWidget *parent = nullptr);
    ~main_window();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void on_popen_button_clicked();

    void on_pclose_button_clicked();

    void on_run_button_clicked();

    void on_pause_button_clicked();

    void on_stop_button_clicked();

    void on_actionExit_triggered();

    void on_yscale_data_currentTextChanged(const QString &arg1);

    void initializeMainWindow();
    void initializePlot();
    void initializeRFScanMode();
    void initializeS11PlotMode();
    void updateXAxisRange(double windowSize);
    void updateYAxisRange(double data, double offset);
    void port_options(bool option);
    void freq_options(bool option);
    void scalingSelection(bool option);
    double scalingYAxisVal(double data);
    UartConfig getUartConfigFromUI();
    void uartWrite();
    void uartRead();
    void rfScan(QByteArray *packet);
    void s11Plot(QByteArray *packet);
    uint64_t convertToHz(double value, const QString &unit);
    void touchStoneParsing(const QString& filePath);

    void on_outDir_data_textChanged(const QString &arg1);

    void on_max_button_clicked();

    void on_min_button_clicked();

    void on_send_button_clicked();

    void on_sParam_data_editingFinished();

    void on_sParam_type_currentTextChanged(const QString &arg1);

private:
    Ui::main_window *ui;

    QSerialPort *hcom1;
    FrequencySettings freqSettings;
    QByteArray rxData;
    QFile logFile;
    QTextStream outLogFile;
    QString logPath;
    bool isReady = false;
    bool isPause = false;
    bool isLog = false;
    uint8_t scaledFactor = 1;
    uint32_t timeIndex = 0;
    double timeVal = 0.0;
    DirectionalCoupler dCoupler;
};
#endif // MAIN_WINDOW_H
