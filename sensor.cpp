#include "sensor.h"

Sensor::Sensor(QString portname, QString identifier, long baudrate, QString name)
{
    qRegisterMetaType<Sensor::SensorStatus>();
    qRegisterMetaType<Sensor::ServiceData>();
    qRegisterMetaType<qint16*>();

    this->portname = portname;
    this->identifier = identifier;
    this->baudrate = baudrate;
    this->name = name;
    port = new QSerialPort(portname);

    if (portname.contains("COM") && portname.mid(3).toInt() != 0)
    {
        setCurrentStatus(Sensor::READY);
    }
    else
    {
        setCurrentStatus(Sensor::PORT_OPEN_ERR);
    }
}

void Sensor::begin()
{
    if (currentStatus == Sensor::READY)
    {
        port->setBaudRate(baudrate);
        port->setPortName(portname);
        port->setParity(QSerialPort::NoParity);
        port->setDataBits(QSerialPort::Data8);
        port->setFlowControl(QSerialPort::NoFlowControl);

        QObject::connect(port,SIGNAL(readyRead()), this, SLOT(readyRead()), Qt::UniqueConnection);

        if (port->open(QIODevice::ReadOnly))
        {
            receiveTimer = new QElapsedTimer();
            timeoutTimer = new QTimer();

            port->clear(QSerialPort::AllDirections);

            QObject::connect(timeoutTimer, SIGNAL(timeout()), this, SLOT(readTimeout()), Qt::UniqueConnection);

            receiveTimer->start();
            timeoutTimer->start(timeout);
            setCurrentStatus(Sensor::BUSY);
            qDebug() << "Sensor" << this->Name() << "initialized in thread" << QThread::currentThreadId();
        }
        else
        {
            setCurrentStatus(Sensor::PORT_OPEN_ERR);
            qDebug() << "Port" << this->name << "open error";
        }
    }
    else
    {        
        qDebug() << "Port" << this->name << "cannot be initialized. Port status is" << currentStatus;
    }
}

void Sensor::terminateThread()
{
    setCurrentStatus(Sensor::TERMINATED);
    this->close();
    qDebug() << "\nThread" << QThread::currentThreadId() << "terminating. Port" << this->name << "closed.";
    emit threadTerminating();
}

void Sensor::close()
{    
    QObject::disconnect(port,SIGNAL(readyRead()), this, SLOT(readyRead()));
    port->clear(QSerialPort::AllDirections);
    QObject::disconnect(timeoutTimer, SIGNAL(timeout()), this, SLOT(readTimeout()));

    timeoutTimer->stop();

    if (timeoutTimer != NULL)
    {
        timeoutTimer->stop();
        delete timeoutTimer;
        timeoutTimer = NULL;
        delete receiveTimer;
        receiveTimer = NULL;
    }
}

void Sensor::readyRead()
{
    int dataSize = messageSize-terminator.length();

    if (timeoutTimer->isActive())
    {
        timeoutTimer->start(timeout);
    }

    rxbuf.append(port->readAll());
    if (rxbuf.size() > messageSize) // 18 bytes of data + 4 bytes timestamp + "\r\n" terminator
    {
        while (rxbuf.indexOf(terminator) != -1)
        {
            cnt++;
            int end = rxbuf.indexOf(terminator);

            if (end-dataSize < 0) // in case of damaged packet, size less than 18+4
            {
                rxbuf = rxbuf.mid(end+2);
                end = rxbuf.indexOf(terminator);
                declinedPackets++;
            }
            else
            {
                QByteArray pack = rxbuf.mid(end-dataSize, dataSize);
                if (pack.size() < dataSize)
                {
                    qDebug() << "\n\nWrong packet size = " << pack.size() << " bytes, in port " << this->name;
                    setCurrentStatus(Sensor::PARSE_ERR);
                }
                else
                {
                    //QDebug deb = qDebug();
                    for (int i = 0; i < 18; i += 2)
                    {
                        databuf[i/2] = (qint16)((unsigned char)(pack[i+1]) << 8 | (unsigned char)(pack[i]));
                        //deb << databuf[i];
                    }
                    timestamp = quint64((unsigned char)(pack[21]) << 24 | (unsigned char)(pack[20]) << 16 | (unsigned char)(pack[19]) << 8 | (unsigned char)(pack[18]));
                }
                rxbuf = rxbuf.mid(end+2);
                emit sendSensorData(databuf);
            }
        }
        if (rxbuf.size() > messageSize)
        {
            qDebug() << "RxBuffer too large after processing with size = " << rxbuf.size() << " bytes, in port " << this->name;
            port->clear(QSerialPort::AllDirections);
        }
        if (cnt >= 1000)
        {
            static ServiceData serviceData;

            serviceData.LocalTimeElapsed = receiveTimer->nsecsElapsed();
            serviceData.RemoteTimeElapsed = timestamp - prev_timestamp;
            serviceData.DeclinedPackets = declinedPackets;

            emit sendSensorServiceData(&serviceData);

            receiveTimer->restart();
            prev_timestamp = timestamp;
            declinedPackets = 0;
            cnt = 0;
        }
    }
}

void Sensor::readTimeout()
{
    setCurrentStatus(Sensor::TIMEOUT_ERR);
    qDebug() << "\nSerial read timeout. Port " << this->name << "closed.";
    this->close();
}

void Sensor::printFromAnotherThread()
{
    qDebug() << "Hello from thread" << QThread::currentThreadId();
}

void Sensor::setCurrentStatus(Sensor::SensorStatus st)
{
    currentStatus = st;
    emit statusChanged(st);
}
