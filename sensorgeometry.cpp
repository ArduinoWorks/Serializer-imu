#include "sensorgeometry.h"

SensorGeometry::SensorGeometry(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<QQuaternion>();
    begin();
}

SensorGeometry::SensorGeometry(QString identifier)
{
    m_identifier = identifier;
}

void SensorGeometry::begin()
{

    Qgyro.setScalar(1);
    Qgyro.setVector(QVector3D(0,0,0));

    GyrCor.setX(0);
    GyrCor.setY(0);
    GyrCor.setZ(0);

    gyrPrev.setX(0);
    gyrPrev.setY(0);
    gyrPrev.setZ(0);

    gyroCalibCounter = 0;
    prevTimestamp = 0;

    magCalibrationData.max.setX(INT32_MIN);
    magCalibrationData.max.setY(INT32_MIN);
    magCalibrationData.max.setZ(INT32_MIN);
    magCalibrationData.min.setX(INT32_MAX);
    magCalibrationData.min.setY(INT32_MAX);
    magCalibrationData.min.setZ(INT32_MAX);

    ready = true;
}

void SensorGeometry::calibrateGyro(qint16 *buf)
{
    if (gyroCalibCounter <= 50)
    {
        GyrCor.setX(GyrCor.x() + (float)buf[3]);
        GyrCor.setY(GyrCor.y() + (float)buf[4]);
        GyrCor.setZ(GyrCor.z() + (float)buf[5]);
        gyroCalibCounter++;
        return;
    }
    else if(!gyroCalibrated)
    {
        gyroCalibrated = true;
        GyrCor.setX(GyrCor.x()/50);
        GyrCor.setY(GyrCor.y()/50);
        GyrCor.setZ(GyrCor.z()/50);
    }
}

void SensorGeometry::calculateNewPose(qint16 *buf, quint64 timestamp)
{
    if (!ready)
        begin();

    if (buf == nullptr)
        return;

    magnetCalibrated = true;

    if (!magnetCalibrated)
        return;

    if (!gyroCalibrated)
    {
        calibrateGyro(buf);
        prevTimestamp = timestamp;
        return;
    }

    QVector3D gyr(buf[3], buf[4], buf[5]);

    gyr.setX(((0.0012207*(gyr.x() - GyrCor.x())) + gyrPrev.x())/2);
    gyr.setY(((0.0012207*(gyr.y() - GyrCor.y())) + gyrPrev.y())/2);
    gyr.setZ(((0.0012207*(gyr.z() - GyrCor.z())) + gyrPrev.z())/2);

    gyrPrev.setX(gyr.x());
    gyrPrev.setY(gyr.y());
    gyrPrev.setZ(gyr.z());

    float dt = ((float)timestamp - prevTimestamp)/1e6;
    prevTimestamp = timestamp;

    QVector3D vspeed(gyr.x()*dt, -gyr.z()*dt, -gyr.y()*dt);
    QQuaternion qspeed = gyro2quat(vspeed);

    Qgyro = Qgyro * qspeed;

    QVector3D Z = QVector3D(buf[0], buf[1], buf[2]); // accelerometer axis
    QVector3D Y = QVector3D::crossProduct(Z, QVector3D(buf[5], buf[7], buf[8])); // accelerometer + magnetometer axis
    QVector3D X = QVector3D::crossProduct(Y, Z); // X axis perpendicular to both

    X.normalize();
    Y.normalize();    
    Z.normalize();

    if (!initialPoseCaptured)
    {
        Qini = QQuaternion::fromAxes(X, Y, Z).inverted();
        initialPoseCaptured = true;
    }
    QQuaternion qref = QQuaternion::fromAxes(X, Y, Z) * Qini;

    //Qgyro = QQuaternion::nlerp(qref, Qgyro, 0.999);

    emit poseChanged(Qgyro);
}

void SensorGeometry::calibrateMagnetometer(qint16 *buf, quint64 timestamp)
{
    if (buf == nullptr)
        return;

    if (!magnetCalibrationFinished)
    {
        QVector3D *point = new QVector3D(buf[6], buf[7], buf[8]);
        magCalibrationData.rawData.append(point);
        setMinMax(point);

        emit sendSingleMagnetMeasure(point);

        if (magCalibrationData.rawData.count() > 50000) // prevent UI lag too many points to render
        {
            stopMagnetometerCalibration();
        }
    }
}

void SensorGeometry::stopMagnetometerCalibration()
{
    magnetCalibrationFinished = true;

    // TODO : save to txt? [mx my mz]

    if (magCalibrationData.rawData.count() > 0 &&
            magCalibrationData.min.x() < magCalibrationData.max.x() &&
            magCalibrationData.min.y() < magCalibrationData.max.y() &&
            magCalibrationData.min.z() < magCalibrationData.max.z())
    {
        qDebug() << "calculate matrix and emit it to Serializer";

        QVector3D bias = getBias();
        magCalibrationData.bias = bias;

        QMatrix3x3 softIron = getSoftIronMatrix(bias);
        getHardIron(softIron, bias);
        qDebug() << magCalibrationData.bias;
        qDebug() << magCalibrationData.matrix;

        magnetCalibrated = true;
    }
    else
    {
        qDebug() << "Failed to calibrate magnetometer : bad raw data";
    }

    qDebug() << "10";
    /*
    while (!magCalibrationData.rawData.isEmpty())
    {
        delete magCalibrationData.rawData.last();
    }
    */
    magnetCalibrationFinished = false;
}

QVector3D SensorGeometry::getBias()
{
    QVector3D bias;
    bias.setX((magCalibrationData.max.x() + magCalibrationData.min.x())/2);
    bias.setY((magCalibrationData.max.y() + magCalibrationData.min.y())/2);
    bias.setZ((magCalibrationData.max.z() + magCalibrationData.min.z())/2);
    return bias;
}

QMatrix3x3 SensorGeometry::getSoftIronMatrix(QVector3D bias)
{
    float alpha = qAsin((magCalibrationData.max.y() - bias.y()) / qSqrt(qPow(magCalibrationData.max.x() - bias.x(), 2) + qPow(magCalibrationData.max.y() - bias.y(), 2)));
    float beta = qAsin((magCalibrationData.max.z() - bias.z()) / qSqrt(qPow(magCalibrationData.max.y() - bias.y(), 2) + qPow(magCalibrationData.max.z() - bias.z(), 2)));
    float gamma = qAsin((magCalibrationData.max.x() - bias.x()) / qSqrt(qPow(magCalibrationData.max.z() - bias.z(), 2) + qPow(magCalibrationData.max.x() - bias.x(), 2)));
    return QQuaternion::fromEulerAngles(beta, alpha, gamma).toRotationMatrix();
}

void SensorGeometry::getHardIron(QMatrix3x3 softIron, QVector3D bias)
{
    for (int i = 0; i < magCalibrationData.rawData.count(); i++)
    {
        QVector3D point = rotate(*magCalibrationData.rawData.at(i), softIron);
        setMinMax(&point);
    }

    const float fnorm = 1000; // norm of magnetic field
    float k[] = { (magCalibrationData.max.x() - bias.x())/ fnorm,
                   (magCalibrationData.max.y() - bias.y())/ fnorm,
                   (magCalibrationData.max.z() - bias.z())/ fnorm };

    for (int row = 0; row < 3; row++)
        for (int col = 0; col <3; col++)
            magCalibrationData.matrix(row,col) = softIron(row,col)*k[row];
}

void SensorGeometry::setMinMax(QVector3D *point)
{
    if (point->x() > magCalibrationData.max.x())
        magCalibrationData.max.setX(point->x());
    if (point->y() > magCalibrationData.max.y())
        magCalibrationData.max.setY(point->y());
    if (point->z() > magCalibrationData.max.z())
        magCalibrationData.max.setZ(point->z());

    if (point->x() < magCalibrationData.min.x())
        magCalibrationData.min.setX(point->x());
    if (point->y() < magCalibrationData.min.y())
        magCalibrationData.min.setY(point->y());
    if (point->z() < magCalibrationData.min.z())
        magCalibrationData.min.setZ(point->z());
}

QVector3D SensorGeometry::rotate(QVector3D point, QMatrix3x3 rm)
{
    return QVector3D(point.x()*rm(0,0) + point.x()*rm(1,0) + + point.x()*rm(2,0),
                     point.y()*rm(0,1) + point.y()*rm(1,1) + + point.y()*rm(2,1),
                     point.z()*rm(0,2) + point.z()*rm(1,2) + + point.z()*rm(2,2));
}


QMatrix3x3 SensorGeometry::angle2dcm(QVector3D gyro)
{
    const float eye[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    QMatrix3x3 dcm(eye);
    QVector<float> c(3);
    QVector<float> s(3);
    QVector<float> V = {gyro.x(), gyro.y(), gyro.z()};

    for (int i =0; i<3; i++)
    {
        c[i] = qCos(V[i]);
        s[i] = qSin(V[i]);
    }
    // XYZ
    // [  cy*cz   sz*cx+sy*sx*cz   sz*sx-sy*cx*cz]
    // [ -cy*sz   cz*cx-sy*sx*sz   cz*sx+sy*cx*sz]
    // [     sy           -cy*sx            cy*cx]
    dcm(0,0)= c[1]*c[2];     dcm(0,1)=s[2]*c[0]+s[1]*s[0]*c[2];     dcm(0,2)=s[2]*s[0]-s[1]*c[0]*c[2];
    dcm(1,0)=-c[1]*s[2];     dcm(1,1)=c[2]*c[0]-s[1]*s[0]*s[2];     dcm(1,2)=c[2]*s[0]+s[1]*c[0]*s[2];
    dcm(2,0)=      s[1];     dcm(2,1)=              -c[1]*s[0];     dcm(2,2)=               c[1]*c[0];

    return dcm;
}

QQuaternion SensorGeometry::gyro2quat(QVector3D gyro)
{
    float theta = gyro.length();
    gyro.normalize();
    QQuaternion q;
    float s = qSin(theta/2);
    q.setVector(s*gyro.x(), s*gyro.y(), s*gyro.z());
    q.setScalar(qCos(theta/2));
    return q;
}