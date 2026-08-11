#ifndef CANFD_DEVICE_H
#define CANFD_DEVICE_H

#include <QObject>
#include <QTimer>
#include <QDebug>
#include <cstdint>

// CAN config: Arbitration 1Mbps, Data 5Mbps (60MHz clock)
// baud = clk / (brp * (1 + tseg1 + tseg2))
// Arb:  60M / (2 * (1+14+3))  = 60M / 36 = 1.667M -> approx 1M with sample point
// Data: 60M / (2 * (1+1+0))   = 60M / 4  = 15M -> with sjw adjustments ~5M

struct CanFdFrame {
    uint32_t id;
    uint8_t  len;       // data length in bytes (0~64)
    uint8_t  data[64];
    bool     is_ext_id;
    bool     is_fd;     // always true for FD frames
    bool     brs;       // bit rate switch
};

class CanFdDevice : public QObject
{
    Q_OBJECT

public:
    explicit CanFdDevice(QObject *parent = nullptr);
    ~CanFdDevice();

    bool open();
    void close();
    bool isOpen() const { return m_opened; }

    bool sendFrame(const CanFdFrame &frame);
    QString lastError() const { return m_lastError; }

signals:
    void frameReceived(const CanFdFrame &frame);
    void deviceStatusChanged(bool connected);

public slots:
    void startReceive();
    void stopReceive();

private slots:
    void pollFrames();

private:
    bool configureDevice();

    bool m_opened;
    QTimer  *m_rxTimer;
    QString  m_lastError;
};

#endif // CANFD_DEVICE_H
