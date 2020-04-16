#include "androidraduimember.h"
#include <QDebug>

AndroidRaduiMember::AndroidRaduiMember(QObject *parent)
    : RadioMemberBase(parent)
{

}

void AndroidRaduiMember::analysisPack(int type, QByteArray msg)
{
    uchar* buff = (uchar*)msg.data();
    int len = msg.length();
//    ushort tep = 0;
//    if(type == 0x04)
#if defined (Q_OS_ANDROID)
    switch (type) {
    case 0x01:{  //心跳
        if(len != 16) break;
        heart_data heart;
        memcpy(&heart, buff, len);
        this->set_chargeState(heart.charge_state);
        this->set_voltage(heart.bat_vol / 10.0);
        this->set_energy(heart.bat_per);
        this->set_time(heart.time/10.0);
        this->set_temperature(heart.temperature);
        this->set_stateOfHealth(heart.state_of_health);
        this->set_rcMode(heart.rc_mode);
        this->set_calirationState(heart.caliration_state);
        this->setVer(heart.verion);
        break;
    }
    case 0x03: { //遥控器各通道 -- 16字节
        if(len != 32) break;
        channels_data channelData;
        memcpy(&channelData, buff, len);
//        qDebug() << "-----channels_data" << channelData.channels[13];
        this->set_channel1(channelData.channels[0]);
        this->set_channel2(channelData.channels[1]);
        this->set_channel3(channelData.channels[2]);
        this->set_channel4(channelData.channels[3]);
        this->set_channel5(channelData.channels[4]);
        this->set_channel6(channelData.channels[5]);
        this->set_channel7(channelData.channels[6]);
        this->set_channel8(channelData.channels[7]);
        this->set_channel9(channelData.channels[8]);
        this->set_channel10(channelData.channels[9]);
        this->set_channel11(channelData.channels[10]);
        this->set_channel12(channelData.channels[11]);
        this->set_channel13(channelData.channels[12]);
        this->set_channel14(channelData.channels[13]);
        this->set_channel15(channelData.channels[14]);
        this->set_channel16(channelData.channels[15]);
        break;
        }
    case 0x04:{  //遥控器校准时各通道值
        if(len != 1) break;
        this->set_checkStatus(*buff++);
        break;
    }
    case 0x05:{  //遥控器校准时各通道值
        if(len != 48) break;
        channels_maxMidMin caliData;
        memcpy(&caliData, buff, len);
        this->set_channelBMid1(caliData.channel1_mid);
        this->set_channelBMid2(caliData.channel2_mid);
        this->set_channelBMid3(caliData.channel3_mid);
        this->set_channelBMid4(caliData.channel4_mid);
        this->set_channelBMid7(caliData.channel7_mid);
        this->set_channelBMid8(caliData.channel8_mid);
        this->set_channelBMax1(caliData.channel1_mid);
        this->set_channelBMax2(caliData.channel2_max);
        this->set_channelBMax3(caliData.channel3_max);
        this->set_channelBMax4(caliData.channel4_max);
        this->set_channelBMax7(caliData.channel7_max);
        this->set_channelBMax8(caliData.channel8_max);
        this->set_channelBMin1(caliData.channel1_min);
        this->set_channelBMin2(caliData.channel2_min);
        this->set_channelBMin3(caliData.channel3_min);
        this->set_channelBMin4(caliData.channel4_min);
        this->set_channelBMin7(caliData.channel7_min);
        this->set_channelBMin8(caliData.channel8_min);
        this->set_channelBVer1(caliData.channel1_current);
        this->set_channelBVer2(caliData.channel2_current);
        this->set_channelBVer3(caliData.channel3_current);
        this->set_channelBVer4(caliData.channel4_current);
        this->set_channelBVer7(caliData.channel7_current);
        this->set_channelBVer8(caliData.channel8_current);
        break;
    }
    case 0x08:{  //单片机唯一ID
        this->setRadioID(QByteArray((char*)buff, 12));
        break;
    }
    default:
        break;
    }
#endif
}
