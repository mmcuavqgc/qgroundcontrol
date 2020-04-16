#ifndef ANDROIDRADUIMEMBER_H
#define ANDROIDRADUIMEMBER_H

#include <QObject>
#include "radiomemberbase.h"

class AndroidRaduiMember : public RadioMemberBase
{
    Q_OBJECT
public:
#pragma pack(push, 1)
    typedef struct
    {
        uint8_t     charge_state; //充电状态
        uint8_t     bat_vol; //电压 * 10的值
        uint8_t     bat_per; //电量百分比
        uint8_t     time; //预计剩余使用时间
        float       temperature; //电池温度
        uint8_t     state_of_health; //电池健康状态
        uint8_t     ground_station_type;
        uint8_t     rc_mode;
        uint8_t     caliration_state;
        uint32_t    verion;  //版本号
    } heart_data;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct{
        uint16_t channels[16];
//        uint16_t aileron;
//        uint16_t elevator;
//        uint16_t throttle;
//        uint16_t rudder;
//        uint16_t flay_mode;
//        uint16_t f1_channel;
//        uint16_t ret_voyage;
//        uint16_t reserve1;
//        uint16_t pitch;
//        uint16_t yaw;
//        uint16_t zoom;
//        uint16_t photo;
//        uint16_t record;
//        uint16_t f2_channel;
//        uint16_t f3_chennel;
//        uint16_t reserve2;
    } channels_data;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct{
        uint16_t channel1_mid;
        uint16_t channel2_mid;
        uint16_t channel3_mid;
        uint16_t channel4_mid;
        uint16_t channel7_mid;
        uint16_t channel8_mid;

        uint16_t channel1_max;
        uint16_t channel2_max;
        uint16_t channel3_max;
        uint16_t channel4_max;
        uint16_t channel7_max;
        uint16_t channel8_max;

        uint16_t channel1_min;
        uint16_t channel2_min;
        uint16_t channel3_min;
        uint16_t channel4_min;
        uint16_t channel7_min;
        uint16_t channel8_min;

        uint16_t channel1_current;
        uint16_t channel2_current;
        uint16_t channel3_current;
        uint16_t channel4_current;
        uint16_t channel7_current;
        uint16_t channel8_current;
    } channels_maxMidMin;
#pragma pack(pop)

    explicit AndroidRaduiMember(QObject *parent = nullptr);

    void analysisPack(int type, QByteArray msg);
    QString radioSettingUrl(){ return "AndroidRadioSettingUrl.qml"; }
signals:

public slots:
};

#endif // ANDROIDRADUIMEMBER_H
