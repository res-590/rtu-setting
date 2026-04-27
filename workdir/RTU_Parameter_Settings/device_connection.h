#ifndef DEVICE_CONNECTION_H
#define DEVICE_CONNECTION_H

#include <QWidget>
#include <QByteArray>
#include <QDebug>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include "qtimer.h"
#include "qthread.h"
namespace Ui {
class Device_connection;
};
class SerialWorkThread : public QThread
{
    Q_OBJECT
    public:
        void run();
        QSerialPort *serial;
        bool hasMessage;
        bool serial_status;
};
class Device_connection : public QWidget
{
    Q_OBJECT

public:
    void sendmessage();
    bool isSerialOpen() const;
    char Serial_sendbuffer[1024];
    char Serial_getbuffer[1024];
    explicit Device_connection(QWidget *parent = nullptr);
    ~Device_connection();
signals:
    void get_Onemessage();
    void send_Onemessage();
    void connectionStatusChanged(bool connected);
public slots:
        void message_handle();
private slots:
    void on_OpenSerialButton_clicked();
    void ReadData();

    void on_SendButton_clicked();

    void on_BackButton_clicked();
private:
    void closeSerial();
    void updateConnectionPanel(bool connected);
    void processReceiveBuffer();
    void dispatchCurrentMessage();
    Ui::Device_connection *ui;
    SerialWorkThread *m_WorkThread;
    SerialWorkThread *m_DeencodeThread;
    QByteArray m_receiveBuffer;
};

#endif // DEVICE_CONNECTION_H
