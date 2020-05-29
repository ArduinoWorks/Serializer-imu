#ifndef SENSOR_H
#define SENSOR_H

#include <QString>
#include <QStringList>
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QThread>
#include <QDebug>
#include <QByteArray>
#include <QMetaType>
#include <QElapsedTimer>

class Sensor : public QObject
{
    Q_OBJECT

public:
    enum SensorStatus
    {
        PARSE_ERROR,
        PORT_OPEN_ERR,
        TIMEOUT,
        READY,
        BUSY,
        TERMINATED
    };    
    Q_ENUM(SensorStatus)

    struct ServiceData
    {
        double LocalTimeElapsed;
        double RemoteTimeElapsed;
        uint   DeclinedPackets;
    };

    explicit Sensor(QObject *parent = nullptr);

    Sensor(QString portname, QString identifier, long baudrate, QString name);

    ~Sensor();

    QString Identifier() { return identifier; }
    QString Portname() { return portname; }
    QString Name() { return name; }
    long Baudrate() { return baudrate; }
    SensorStatus CurrentStatus() { return currentStatus; }

private:

    const QString terminator = "\r\n";    
    const long timeout = 5000;
    const int messageSize = 24;

    long baudrate;
    QString identifier;
    QString portname;
    QString name;
    QSerialPort* port;
    QElapsedTimer* receiveTimer;
    QTimer* timeoutTimer;
    SensorStatus currentStatus;

    void open();
    void setCurrentStatus(SensorStatus st);

public slots:
    void begin();
    void close();
    void terminateThread();

private slots:
    void readyRead();
    void readTimeout();
    void printFromAnotherThread();    

signals:
    void threadTerminating();
    void sendSensorData(qint16 *databuf);
    void sendSensorServiceData(Sensor::ServiceData*);
    void statusChanged(SensorStatus st);
};

Q_DECLARE_METATYPE(Sensor::SensorStatus)
Q_DECLARE_METATYPE(Sensor::ServiceData)

#endif // SENSOR_H
