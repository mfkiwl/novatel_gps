#include "novatel_gps.h"
#include <thread>
#include <chrono>

/*************************** CRC functions (Firmware Reference Manual, p.32 + APN-030 Rev 1 Application Note) ***************************/

inline uint32_t ByteSwap (uint32_t n)
{ 
   return ( ((n & 0x000000FF)<<24) + ((n & 0x0000FF00)<<8) + ((n & 0x00FF0000)>>8) + (( n & 0xFF000000)>>24) );
}

#define CRC32_POLYNOMIAL    0xEDB88320L
#define MAX_SYNC_FAIL       25

/* --------------------------------------------------------------------------
Calculate a CRC value to be used by CRC calculation functions.
-------------------------------------------------------------------------- */
inline uint32_t CRC32Value(int i)
{
    int j;
    uint32_t ulCRC;
    ulCRC = i;

    for ( j = 8 ; j > 0; j-- )
    {
        if ( ulCRC & 1 )
            ulCRC = ( ulCRC >> 1 ) ^ CRC32_POLYNOMIAL;
        else
            ulCRC >>= 1;
    }
    return ulCRC;
}

/* --------------------------------------------------------------------------
Calculates the CRC-32 of a block of data all at once
-------------------------------------------------------------------------- */
inline uint32_t CalculateBlockCRC32
(
    uint32_t ulCount,  /* Number of bytes in the data block */
    uint8_t *ucBuffer /* Data block */
)
{
    uint32_t ulTemp1;
    uint32_t ulTemp2;
    uint32_t ulCRC = 0;

    while ( ulCount-- != 0 )
    {
        ulTemp1 = ( ulCRC >> 8 ) & 0x00FFFFFFL;
        ulTemp2 = CRC32Value( ((int) ((ulCRC) ^ (*ucBuffer)) ) & 0xff );
        ucBuffer++;
        ulCRC = ulTemp1 ^ ulTemp2;
    }
    return( ulCRC );
}


// GPS Class methods

GPS::GPS() : GPS_PACKET_SIZE(700),
    serial_port_("/dev/ttyUSB0"),
    gps_week_(0),
    gps_week_1024_(0),
    gps_secs_(0),

    D_SYNC0(0xAA),
    D_SYNC1(0x44),
    D_SYNC2(0x12),
    D_HDR_LEN(28),

    // Message Header, Firmware Reference Manual pg. 23
    D_HDR       (3),
    D_MSG_ID    (4),
    D_MSG_TP    (6),
    D_PORT_ADD  (7),
    D_MSG_LEN   (8),
    D_SEQ       (10),
    D_IDLE_T    (12),
    D_TIME_ST   (13),
    D_G_WEEK    (14),
    D_G_MS      (16),
    D_RCV_ST    (20),
    D_RCV_SW_V  (26),

    // BESTXYZ Log, Firmware Reference Manual pg. 264
    BXYZ_PSTAT      (D_HDR_LEN),
    BXYZ_PTYPE      (D_HDR_LEN + 4),
    BXYZ_PX         (D_HDR_LEN + 8),
    BXYZ_PY         (D_HDR_LEN + 16),
    BXYZ_PZ         (D_HDR_LEN + 24),
    BXYZ_sPX        (D_HDR_LEN + 32),
    BXYZ_sPY        (D_HDR_LEN + 36),
    BXYZ_sPZ        (D_HDR_LEN + 40),
    BXYZ_VSTAT      (D_HDR_LEN + 44),
    BXYZ_VTYPE      (D_HDR_LEN + 48),
    BXYZ_VX         (D_HDR_LEN + 52),
    BXYZ_VY         (D_HDR_LEN + 60),
    BXYZ_VZ         (D_HDR_LEN + 68),
    BXYZ_sVX        (D_HDR_LEN + 76),
    BXYZ_sVY        (D_HDR_LEN + 80),
    BXYZ_sVZ        (D_HDR_LEN + 84),
    BXYZ_STNID      (D_HDR_LEN + 88),
    BXYZ_VLATE      (D_HDR_LEN + 92),
    BXYZ_DIFFAGE    (D_HDR_LEN + 96),
    BXYZ_SOLAGE     (D_HDR_LEN + 100),
    BXYZ_SV         (D_HDR_LEN + 104),
    BXYZ_SOLSV      (D_HDR_LEN + 106),

    // BESTPOS Log, Firmware Reference Manual pg. 256
    BESTPOS_SOLSTAT     (D_HDR_LEN),
    BESTPOS_POSTYPE     (D_HDR_LEN + 4),
    BESTPOS_LAT         (D_HDR_LEN + 8),
    BESTPOS_LONG        (D_HDR_LEN + 16),
    BESTPOS_HGT         (D_HDR_LEN + 24),
    BESTPOS_UND         (D_HDR_LEN + 32),
    BESTPOS_DATUMID     (D_HDR_LEN + 36),
    BESTPOS_SLAT        (D_HDR_LEN + 40),
    BESTPOS_SLON        (D_HDR_LEN + 44),
    BESTPOS_SHGT        (D_HDR_LEN + 48),
    BESTPOS_STNID       (D_HDR_LEN + 52),
    BESTPOS_DIFFAGE     (D_HDR_LEN + 56),
    BESTPOS_SOLAGE      (D_HDR_LEN + 60),
    BESTPOS_SV          (D_HDR_LEN + 64),
    BESTPOS_SOLNSV      (D_HDR_LEN + 65),
    BESTPOS_GGL1        (D_HDR_LEN + 66),
    BESTPOS_GGL1L2      (D_HDR_LEN + 67),
    BESTPOS_EXTSOLSTAT  (D_HDR_LEN + 69),
    BESTPOS_SIGMASK     (D_HDR_LEN + 71),

    // SATXYZ Log, Firmware Reference Manual pg. 562
    SATXYZ_NSAT         (D_HDR_LEN + 8),
    SATXYZ_PRN          (D_HDR_LEN + 12),
    SATXYZ_X            (D_HDR_LEN + 16),
    SATXYZ_Y            (D_HDR_LEN + 24),
    SATXYZ_Z            (D_HDR_LEN + 32),
    SATXYZ_CLKCORR      (D_HDR_LEN + 40),
    SATXYZ_IONCORR      (D_HDR_LEN + 48),
    SATXYZ_TRPCORR      (D_HDR_LEN + 56),
    SATXYZ_OFFSET       (68),

    // TRACKSTAT Log, Firmware Reference Manual pg. 568
    TRACKSTAT_SOLSTAT   (D_HDR_LEN),
    TRACKSTAT_POSTYPE   (D_HDR_LEN + 4),
    TRACKSTAT_CUTOFF    (D_HDR_LEN + 8),
    TRACKSTAT_CHAN      (D_HDR_LEN + 12),
    TRACKSTAT_PRN       (D_HDR_LEN + 16),
    TRACKSTAT_TRKSTAT   (D_HDR_LEN + 20),
    TRACKSTAT_PSR       (D_HDR_LEN + 24),
    TRACKSTAT_DOPPLER   (D_HDR_LEN + 32),
    TRACKSTAT_CNo       (D_HDR_LEN + 36),
    TRACKSTAT_LOCKTIME  (D_HDR_LEN + 40),
    TRACKSTAT_PSRRES    (D_HDR_LEN + 44),
    TRACKSTAT_REJECT    (D_HDR_LEN + 48),
    TRACKSTAT_PSRW      (D_HDR_LEN + 52),
    TRACKSTAT_OFFSET    (40),

    // RANGE Log, Firmware Reference Manual pg. 403
    RANGE_OBS       (D_HDR_LEN),
    RANGE_PRN       (D_HDR_LEN + 4),
    RANGE_PSR       (D_HDR_LEN + 8),
    RANGE_PSR_STD   (D_HDR_LEN + 16),
    RANGE_ADR       (D_HDR_LEN + 20),
    RANGE_ADR_STD   (D_HDR_LEN + 28),
    RANGE_DOPPLER   (D_HDR_LEN + 32),
    RANGE_CNo       (D_HDR_LEN + 36),
    RANGE_LOCKTIME  (D_HDR_LEN + 40),
    RANGE_TRKSTART  (D_HDR_LEN + 44),
    RANGE_OFFSET    (44),

    S_MSG_ID    (2),
    S_MSG_LEN   (2),
    S_SEQ_NUM   (2),
    S_T_WEEK    (2),
    S_T_MS      (4),
    S_GPS_STATUS(4),
    S_RESERVED  (2),
    S_SW_VERS   (2),
    S_CRC       (4),

    TIMEOUT_US(1e4),
    OLD_BPS   (9600),
    BPS       (115200),
    MAX_BYTES (1000),

    gps_data_(GPS_PACKET_SIZE, 0),
    velocity_(3, 0),
    sigma_position_(3, 0),
    sigma_velocity_(3, 0),

    latitude_ (0.0),
    longitude_(0.0),
    altitude_ (0.0),
    stdev_latitude_ (0.),
    stdev_longitude_(0.),
    stdev_altitude_ (0.),
    covar_latitude_ (0.),
    covar_longitude_(0.),
    covar_altitude_ (0.)
{
}

GPS::~GPS()
{
    close();
}

void GPS::init(int log_id)
{
    int err;

    waitReceiveInit();

    // Init serial port at 9600 bps
    if((err = serialcom_init(&gps_SerialPortConfig_, 1, (char*)serial_port_.c_str(), OLD_BPS)) != SERIALCOM_SUCCESS)
    {
        ROS_ERROR_STREAM("serialcom_init failed " << err);
        throwSerialComException(err);
    }

    // Configure GPS, set baudrate to 115200 bps and reconnect
    ROS_INFO("Configuring Receiver");
    configure();

    // Request GPS data
    char buf[100];
    double time_f = static_cast<double>(1.0/rate_);
    if(log_id == BESTPOS)
    {
        sprintf(buf, "LOG BESTPOSB ONTIME %f", time_f);
        command(buf);
    }
    if(log_id == BESTXYZ)
    {
        sprintf(buf, "LOG BESTXYZB ONTIME %f", time_f);
        command(buf);
    }
    if(log_id == TRACKSTAT)
    {
        sprintf(buf, "LOG TRACKSTATB ONTIME %f", time_f);
        command(buf);
    }
    if(log_id == SATXYZ)
    {
        sprintf(buf, "LOG SATXYZB ONTIME %f", time_f);
        command(buf);
    }
    if(log_id == RANGE)
    {
        sprintf(buf, "LOG RANGEB ONTIME %f", time_f);
        command(buf);
    }
    if(log_id == -1)
    {
        std::this_thread::sleep_for( std::chrono::milliseconds(5) );
        sprintf(buf, "LOG BESTXYZB ONTIME %f", time_f);
        command(buf);
        std::this_thread::sleep_for( std::chrono::milliseconds(5) );
        sprintf(buf, "LOG TRACKSTATB ONTIME %f", time_f);
        command(buf);
        std::this_thread::sleep_for( std::chrono::milliseconds(5) );
        sprintf(buf, "LOG SATXYZB ONTIME %f", time_f);
        command(buf);
        std::this_thread::sleep_for( std::chrono::milliseconds(5) );
        sprintf(buf, "LOG RANGEB ONTIME %f", time_f);
        command(buf);
    }
}

void GPS::init(int log_id, std::string port, double rate = 20)
{
    int err;

    if(port != std::string())
        serial_port_ = port;

    rate_ = rate;
    TIMEOUT_US = ((1.0/rate_)*1e6);
    ROS_INFO_STREAM("TIMEOUT_US = " << TIMEOUT_US);

    // Init serial port at 9600 bps
    if((err = serialcom_init(&gps_SerialPortConfig_, 1, (char*)serial_port_.c_str(), OLD_BPS)) != SERIALCOM_SUCCESS)
    {
        ROS_ERROR_STREAM("serialcom_init failed " << err);
        throwSerialComException(err);
    }

    waitReceiveInit();
    // Configure GPS, set baudrate to 115200 bps and reconnect
    ROS_INFO("Configuring Receiver");
    configure();

    // Request GPS data
    char buf[100];
    double time_f = static_cast<double>(1.0/rate_);
    if(log_id == BESTPOS)
    {
        sprintf(buf, "LOG BESTPOSB ONTIME %f", time_f);
        command(buf);
    }
    if(log_id == BESTXYZ)
    {
        sprintf(buf, "LOG BESTXYZB ONTIME %f", time_f);
        command(buf);
    }
    if(log_id == TRACKSTAT)
    {
        sprintf(buf, "LOG TRACKSTATB ONTIME %f", time_f);
        command(buf);
    }
    if(log_id == SATXYZ)
    {
        sprintf(buf, "LOG SATXYZB ONTIME %f", time_f);
        command(buf);
    }
    if(log_id == RANGE)
    {
        sprintf(buf, "LOG RANGEB ONTIME %f", time_f);
        command(buf);
    }
    if(log_id == -1)
    {
        std::this_thread::sleep_for( std::chrono::milliseconds(5) );
        sprintf(buf, "LOG BESTXYZB ONTIME %f", time_f);
        command(buf);
        std::this_thread::sleep_for( std::chrono::milliseconds(5) );
        sprintf(buf, "LOG TRACKSTATB ONTIME %f", time_f);
        command(buf);
        std::this_thread::sleep_for( std::chrono::milliseconds(5) );
        sprintf(buf, "LOG SATXYZB ONTIME %f", time_f);
        command(buf);
        std::this_thread::sleep_for( std::chrono::milliseconds(5) );
        sprintf(buf, "LOG RANGEB ONTIME %f", time_f);
        command(buf);
    }
}

void GPS::waitReceiveInit()
{
    ROS_INFO("Wainting to Receiver initialize");
    std::this_thread::sleep_for( std::chrono::seconds(11) );
}

int GPS::readDataFromReceiver()
{
    int err;
    int data_ready = 0;
    // State machine variables
    int b = 0, bb = 0, s = GPS_SYNC_ST;

    // Storage for data read from serial port
    uint8_t data_read;

    // Multi-byte data
    uint16_t msg_id, msg_len, t_week;
    uint32_t t_ms, crc_from_packet;

    // Try to sync with IMU and get latest data packet, up to MAX_BYTES read until failure
    for(int i = 0; (!data_ready)&&(i < MAX_BYTES); i++)
    {
        // Read data from serial port
        if((err = serialcom_receivebyte(&gps_SerialPortConfig_, &data_read, TIMEOUT_US)) != SERIALCOM_SUCCESS)
        {
            ROS_ERROR_STREAM("serialcom_receivebyte failed " << err);
            break;
        }

        // Parse GPS packet (Firmware Reference Manual, p.22)
        switch(s)
        {
            case GPS_SYNC_ST:
            {
                // State logic: Packet starts with 3 sync bytes with values 0xAA, 0x44, 0x12
                switch(b)
                {
                    case SYNC0:
                    {
                        if(data_read == D_SYNC0)
                        {
                            gps_data_[b] = data_read;
                            b++;
                        }
                        else
                            // Out of sync, reset
                            b = 0;
                        break;
                    }

                    case SYNC1:
                    {
                        if(data_read == D_SYNC1)
                        {
                            gps_data_[b] = data_read;
                            b++;
                        }
                        else
                            // Out of sync, reset
                            b = 0;
                        break;
                    }

                    case SYNC2:
                    {
                        if(data_read == D_SYNC2)
                        {
                            gps_data_[b] = data_read;
                            b++;
                        }
                        else
                            // Out of sync, reset
                            b = 0;
                        break;
                    }
                }
                // State transition: I have reached the HDR_LEN byte without resetting
                if(b == HDR_LEN)
                    s = GPS_HEADER_ST;
            }
            break;

            case GPS_HEADER_ST:
            {
                // State logic: HDR_LEN, MSG_ID, MSG_TYPE, PORT_ADDR, MSG_LEN, SEQ_NUM, IDLE_T, T_STATUS, T_WEEK, T_MS, GPS_STATUS, RESERVED, SW_VERS
                switch(b)
                {
                    case HDR_LEN:
                    {
                        if(data_read == D_HDR_LEN)
                        {
                            gps_data_[b] = data_read;
                            b++;
                        }
                        else
                        {
                            // Invalid HDR_LEN, reset
                            ROS_ERROR("invalid HDR_LEN %d", data_read);
                            b = 0;
                            s = GPS_SYNC_ST;
                        }
                        break;
                    }

                    case MSG_ID:
                    {
                        // Index bb is for bytes in multi-byte variables
                        gps_data_[b+bb] = data_read;
                        bb++;

                        if(bb == S_MSG_ID)
                        {
                            // Merge bytes and process
                            memcpy(&msg_id, &gps_data_[MSG_ID], sizeof(uint16_t));

                            // Update byte indices
                            bb = 0;
                            b += S_MSG_ID;
                        }
                        break;
                    }

                    case MSG_TYPE:
                    {
                        gps_data_[b] = data_read;
                        b++;
                        break;
                    }

                    case PORT_ADDR:
                    {
                        gps_data_[b] = data_read;
                        b++;
                        break;
                    }

                    case MSG_LEN:
                    {
                        // Index bb is for bytes in multi-byte variables
                        gps_data_[b+bb] = data_read;
                        bb++;

                        if(bb == S_MSG_LEN)
                        {
                            // Merge bytes and process
                            memcpy(&msg_len, &gps_data_[MSG_LEN], sizeof(uint16_t));
                            // ROS_INFO("Message Length = %d", msg_len);
                            // I was having some problems with (msg_len == 0)...
                            if(msg_len != 0)
                            {
                                // Update byte indices
                                bb = 0;
                                b += S_MSG_LEN;
                            }
                            else
                            {
                                // Something wrong, reset
                                bb = 0;
                                b = 0;
                                s = GPS_SYNC_ST;
                            }
                        }
                        break;
                    }

                    case SEQ_NUM:
                    {
                        // Index bb is for bytes in multi-byte variables
                        gps_data_[b+bb] = data_read;
                        bb++;

                        if(bb == S_SEQ_NUM)
                        {
                            // Merge bytes and process
                            //memcpy(&seq_num, &gps_data_[SEQ_NUM], sizeof(uint16_t));

                            // Update byte indices
                            bb = 0;
                            b += S_SEQ_NUM;
                        }
                        break;
                    }

                    case IDLE_T:
                    {
                        gps_data_[b] = data_read;
                        b++;
                        break;
                    }

                    case T_STATUS:
                    {
                        gps_data_[b] = data_read;
                        b++;
                        break;
                    }

                    case T_WEEK:
                    {
                        // Index bb is for bytes in multi-byte variables
                        gps_data_[b+bb] = data_read;
                        bb++;

                        if(bb == S_T_WEEK)
                        {
                            // Merge bytes and process
                            memcpy(&t_week, &gps_data_[T_WEEK], sizeof(uint16_t));

                            // Update byte indices
                            bb = 0;
                            b += S_T_WEEK;
                        }
                        break;
                    }

                    case T_MS:
                    {
                        // Index bb is for bytes in multi-byte variables
                        gps_data_[b+bb] = data_read;
                        bb++;

                        if(bb == S_T_MS)
                        {
                            // Merge bytes and process
                            memcpy(&t_ms, &gps_data_[T_MS], sizeof(uint32_t));

                            // Update byte indices
                            bb = 0;
                            b += S_T_MS;
                        }
                        break;
                    }

                    case GPS_STATUS:
                    {
                        // Index bb is for bytes in multi-byte variables
                        gps_data_[b+bb] = data_read;
                        bb++;

                        if(bb == S_GPS_STATUS)
                        {
                            // Merge bytes and process
                            memcpy(&(status_), &gps_data_[GPS_STATUS], sizeof(uint32_t));

                            // Update byte indices
                            bb = 0;
                            b += S_GPS_STATUS;
                        }
                        break;
                    }

                    case RESERVED:
                    {   
                        // Index bb is for bytes in multi-byte variables
                        // Skip this section, no useful data
                        bb++;

                        if(bb == S_RESERVED)
                        {
                            // Update byte indices
                            bb = 0;
                            b += S_RESERVED;
                        }
                        break;
                    }

                    case SW_VERS:
                    {
                        // Index bb is for bytes in multi-byte variables
                        gps_data_[b+bb] = data_read;
                        bb++;

                        if(bb == S_SW_VERS)
                        {
                            // Merge bytes and process
                            //memcpy(&sw_vers, &gps_data_[SW_VERS], sizeof(uint16_t));

                            // Update byte indices
                            bb = 0;
                            b += S_SW_VERS;
                        }
                        break;
                    }
                }

                // State transition: I have reached the DATA bytes without resetting
                if(b == DATA)
                    s = GPS_PAYLOAD_ST;  
            }
            break;

            case GPS_PAYLOAD_ST:
            {
                // State logic: Grab data until you reach the CRC bytes
                gps_data_[b+bb] = data_read;
                bb++;

                // State transition: I have reached the CRC bytes
                if(bb == msg_len)
                {
                    // Bytes are decoded after CRC check

                    // Update byte indices
                    bb = 0;
                    b += msg_len;
                    s = GPS_CRC_ST;
                }
            }
            break;

            case GPS_CRC_ST:
            {
                // Index bb is for bytes in multi-byte variables
                gps_data_[b+bb] = data_read;
                bb++;
                if(bb == S_CRC)
                {
                    uint32_t crc_calculated;
                    // Grab CRC from packet
                    crc_from_packet = static_cast<uint32_t>((gps_data_[b+3] << 24) | (gps_data_[b+2] << 16) | (gps_data_[b+1] << 8) | gps_data_[b]);

                    // Calculate CRC from packet (b = packet size)
                    // crc_calculated = CalculateBlockCRC32(b, &gps_data_[0]); // GAMBIARRA
                    crc_calculated = CalculateBlockCRC32(b, gps_data_.data()); // C++11

                    // Compare them to see if valid packet 
                    if(crc_from_packet != ByteSwap(crc_calculated))
                    {
                        // ROS_ERROR("CRC does not match (%0x != %0x)", crc_from_packet, ByteSwap(crc_calculated));
                    }
                    decode(msg_id);
                    data_ready = 1;

                    // State transition: Unconditional reset
                    bb = 0;
                    b = 0;
                    s = GPS_SYNC_ST;
                }
            }
            break;
        }
    }

    // Flush port and request more data
    // tcflush(gps_SerialPortConfig_.fd, TCIOFLUSH);
    // command("LOG BESTXYZB ONCE");
    return data_ready;
}

void GPS::decode(uint16_t msg_id)
{
    ROS_INFO("Message ID = %u", msg_id);
    ROS_ASSERT_MSG(sizeof(double) == 8, "sizeof(double) != 8, check your compiler");
    ROS_ASSERT_MSG(sizeof(float) == 4, "sizeof(double) != 4, check your compiler");

    // Reading message header
    memcpy(&msg_header_.msg_id, &gps_data_[D_MSG_ID], sizeof(uint16_t));
    memcpy(&msg_header_.msg_len, &gps_data_[D_MSG_LEN], sizeof(uint16_t));
    memcpy(&msg_header_.seq, &gps_data_[D_SEQ], sizeof(uint16_t));
    memcpy(&msg_header_.idle_t, &gps_data_[D_IDLE_T], sizeof(uint8_t));
    memcpy(&time_stat_, &gps_data_[D_TIME_ST], sizeof(uint8_t));
    memcpy(&msg_header_.gps_week, &gps_data_[D_G_WEEK], sizeof(uint16_t));
    memcpy(&msg_header_.gps_ms, &gps_data_[D_G_MS], sizeof(uint32_t));
    memcpy(&msg_header_.rcv_stat_n, &gps_data_[D_RCV_ST], sizeof(uint16_t));
    memcpy(&msg_header_.rcv_sw_v, &gps_data_[D_RCV_SW_V], sizeof(uint16_t));

    // Nible 0
    msg_header_.rcv_stat.error = (msg_header_.rcv_stat_n & 0x00000001);
    msg_header_.rcv_stat.temp_err = ((msg_header_.rcv_stat_n & 0x00000002) >> 1);
    msg_header_.rcv_stat.vol_err = ((msg_header_.rcv_stat_n & 0x00000004) >> 2);
    msg_header_.rcv_stat.ant_pwr_err = ((msg_header_.rcv_stat_n & 0x00000008) >> 3);

    // Nible 1
    msg_header_.rcv_stat.ant_open_err = ((msg_header_.rcv_stat_n & 0x00000020) >> 5);
    msg_header_.rcv_stat.ant_short_err = ((msg_header_.rcv_stat_n & 0x00000040) >> 6);
    msg_header_.rcv_stat.cpu_over = ((msg_header_.rcv_stat_n & 0x00000080) >> 7);

    // Nible 2
    msg_header_.rcv_stat.com1_ovr_err = ((msg_header_.rcv_stat_n & 0x00000100) >> 8);
    msg_header_.rcv_stat.com2_ovr_err = ((msg_header_.rcv_stat_n & 0x00000200) >> 9);
    msg_header_.rcv_stat.com3_ovr_err = ((msg_header_.rcv_stat_n & 0x00000400) >> 10);
    msg_header_.rcv_stat.usb_ovr_err  = ((msg_header_.rcv_stat_n & 0x00000800) >> 11);

    // Nible 4
    msg_header_.rcv_stat.rf1_err  = ((msg_header_.rcv_stat_n & 0x00008000) >> 15);
    msg_header_.rcv_stat.rf2_err  = ((msg_header_.rcv_stat_n & 0x00020000) >> 17);
    msg_header_.rcv_stat.alm_utc_err  = ((msg_header_.rcv_stat_n & 0x00040000) >> 18);
    msg_header_.rcv_stat.pos_sol_err  = ((msg_header_.rcv_stat_n & 0x00080000) >> 19);

    // Nible 5
    msg_header_.rcv_stat.pos_fixed  = ((msg_header_.rcv_stat_n & 0x00100000) >> 20);
    msg_header_.rcv_stat.clk_st_err  = ((msg_header_.rcv_stat_n & 0x00200000) >> 21);
    msg_header_.rcv_stat.clk_err  = ((msg_header_.rcv_stat_n & 0x00400000) >> 22);
    msg_header_.rcv_stat.osc_ext  = ((msg_header_.rcv_stat_n & 0x00800000) >> 23);

    // Nible 7
    msg_header_.rcv_stat.soft_err  = ((msg_header_.rcv_stat_n & 0x01000000) >> 24);
    msg_header_.rcv_stat.aux3_err  = ((msg_header_.rcv_stat_n & 0x20000000) >> 29);
    msg_header_.rcv_stat.aux2_err  = ((msg_header_.rcv_stat_n & 0x40000000) >> 30);
    msg_header_.rcv_stat.aux1_err  = ((msg_header_.rcv_stat_n & 0x80000000) >> 31);

    msg_header_.time_stat.time_stat = time_stat_;

    // ROS_INFO_STREAM("msg_id " << msg_header_.msg_id << "\n" << 
    //                 "msg len " << msg_header_.msg_len << "\n" << 
    //                 "seq " << msg_header_.seq << "\n" << 
    //                 "idle_t " << (int)msg_header_.idle_t << "\n" << 
    //                 "time_stat " << msg_header_.time_stat.time_stat << "\n" << 
    //                 "gps_week_ " << msg_header_.gps_week_ << "\n" <<
    //                 "gps_ms " << msg_header_.gps_ms);

    if(msg_id == BESTXYZ)
    {
        memcpy(&position_status_, &gps_data_[BXYZ_PSTAT], sizeof(uint16_t));
        memcpy(&position_type_, &gps_data_[BXYZ_PTYPE], sizeof(uint16_t));

        memcpy(&x_, &gps_data_[BXYZ_PX], sizeof(double));
        memcpy(&y_, &gps_data_[BXYZ_PY], sizeof(double));
        memcpy(&z_, &gps_data_[BXYZ_PZ], sizeof(double));

        memcpy(&sigma_position_[0], &gps_data_[BXYZ_sPX], sizeof(float));
        memcpy(&sigma_position_[1], &gps_data_[BXYZ_sPY], sizeof(float));
        memcpy(&sigma_position_[2], &gps_data_[BXYZ_sPZ], sizeof(float));

        memcpy(&velocity_status_, &gps_data_[BXYZ_VSTAT], sizeof(uint16_t));
        memcpy(&velocity_type_, &gps_data_[BXYZ_VTYPE], sizeof(uint16_t));

        memcpy(&velocity_[0], &gps_data_[BXYZ_VX], sizeof(double));
        memcpy(&velocity_[1], &gps_data_[BXYZ_VY], sizeof(double));
        memcpy(&velocity_[2], &gps_data_[BXYZ_VZ], sizeof(double));

        memcpy(&sigma_velocity_[0], &gps_data_[BXYZ_sVX], sizeof(float));
        memcpy(&sigma_velocity_[1], &gps_data_[BXYZ_sVY], sizeof(float));
        memcpy(&sigma_velocity_[2], &gps_data_[BXYZ_sVZ], sizeof(float));

        memcpy(&number_sat_track_, &gps_data_[BXYZ_SV], sizeof(uint8_t));
        memcpy(&number_sat_sol_, &gps_data_[BXYZ_SOLSV], sizeof(uint8_t));

        // ROS_INFO("Position Solution Status = %d", position_status_);
        // ROS_INFO("Velocity Solution Status = %d", velocity_status_);

        // ROS_INFO("Sat = %d", number_sat_track_);
        // ROS_INFO("Sat sol = %d", number_sat_sol_);
    }
    if(msg_id == BESTPOS)
    {
        memcpy(&solution_status_, &gps_data_[BESTPOS_SOLSTAT], sizeof(uint16_t));
        memcpy(&position_type_, &gps_data_[BESTPOS_POSTYPE], sizeof(uint16_t));

        memcpy(&latitude_, &gps_data_[BESTPOS_LAT], sizeof(double));
        memcpy(&longitude_, &gps_data_[BESTPOS_LONG], sizeof(double));
        memcpy(&altitude_, &gps_data_[BESTPOS_HGT], sizeof(double));

        memcpy(&stdev_latitude_, &gps_data_[BESTPOS_SLAT], sizeof(float));
        memcpy(&stdev_longitude_, &gps_data_[BESTPOS_SLON], sizeof(float));
        memcpy(&stdev_altitude_, &gps_data_[BESTPOS_SHGT], sizeof(float));

        memcpy(&number_sat_track_, &gps_data_[BESTPOS_SV], sizeof(uint8_t));
        memcpy(&number_sat_sol_, &gps_data_[BESTPOS_SOLNSV], sizeof(uint8_t));

        ROS_INFO("Sat = %d", number_sat_track_);
        ROS_INFO("Sat sol = %d", number_sat_sol_);
        covar_latitude_ = stdev_latitude_ * stdev_latitude_;
        covar_longitude_ = stdev_longitude_ * stdev_longitude_;
        covar_altitude_ = stdev_altitude_ * stdev_altitude_;
    }
    if(msg_id == SATXYZ)
    {
        memcpy(&number_satellites_, &gps_data_[SATXYZ_NSAT], sizeof(uint32_t));
        // ROS_INFO("Number of satellites %d", number_satellites_);
        satellites_.satellites.resize(number_satellites_);

        for(int i = 0; i < number_satellites_; ++i)
        {
            // PRN
            memcpy(&satellites_.satellites[i].prn_slot, &gps_data_[SATXYZ_PRN + i*SATXYZ_OFFSET], sizeof(uint32_t));

            // Satellite position
            memcpy(&satellites_.satellites[i].position.x, &gps_data_[SATXYZ_X + i*SATXYZ_OFFSET], sizeof(double));
            memcpy(&satellites_.satellites[i].position.y, &gps_data_[SATXYZ_Y + i*SATXYZ_OFFSET], sizeof(double));
            memcpy(&satellites_.satellites[i].position.z, &gps_data_[SATXYZ_Z + i*SATXYZ_OFFSET], sizeof(double));

            // Corrections
            memcpy(&satellites_.satellites[i].clk_corr, &gps_data_[SATXYZ_CLKCORR + i*SATXYZ_OFFSET], sizeof(double));
            memcpy(&satellites_.satellites[i].ion_corr, &gps_data_[SATXYZ_IONCORR + i*SATXYZ_OFFSET], sizeof(double));
            memcpy(&satellites_.satellites[i].trop_corr, &gps_data_[SATXYZ_TRPCORR + i*SATXYZ_OFFSET], sizeof(double));
        }
    }
    if(msg_id == TRACKSTAT)
    {
        memcpy(&tracking_.solution_status, &gps_data_[TRACKSTAT_SOLSTAT], sizeof(uint32_t));
        memcpy(&tracking_.position_type, &gps_data_[TRACKSTAT_POSTYPE], sizeof(uint32_t));
        memcpy(&tracking_.cutoff, &gps_data_[TRACKSTAT_CUTOFF], sizeof(float));
        memcpy(&tracking_.channels, &gps_data_[TRACKSTAT_CHAN], sizeof(int32_t));
        // ROS_INFO("Channels = %d", tracking_.channels);
        tracking_.channel.resize(tracking_.channels);

        for(int i = 0; i < tracking_.channels; ++i)
        {
            memcpy(&tracking_.channel[i].prn_slot, &gps_data_[TRACKSTAT_PRN + i*TRACKSTAT_OFFSET], sizeof(int16_t));
            memcpy(&tracking_.channel[i].ch_tr_status, &gps_data_[TRACKSTAT_TRKSTAT + i*TRACKSTAT_OFFSET], sizeof(uint32_t));

            memcpy(&tracking_.channel[i].psr, &gps_data_[TRACKSTAT_PSR + i*TRACKSTAT_OFFSET], sizeof(double));
            memcpy(&tracking_.channel[i].doppler, &gps_data_[TRACKSTAT_DOPPLER + i*TRACKSTAT_OFFSET], sizeof(float));

            memcpy(&tracking_.channel[i].cn0, &gps_data_[TRACKSTAT_CNo + i*TRACKSTAT_OFFSET], sizeof(float));
            memcpy(&tracking_.channel[i].locktime, &gps_data_[TRACKSTAT_LOCKTIME + i*TRACKSTAT_OFFSET], sizeof(float));

            memcpy(&tracking_.channel[i].psr_res, &gps_data_[TRACKSTAT_PSRRES + i*TRACKSTAT_OFFSET], sizeof(float));
            memcpy(&tracking_.channel[i].psr_weight, &gps_data_[TRACKSTAT_PSRW + i*TRACKSTAT_OFFSET], sizeof(float));

            tracking_.channel[i].tracking_status.trck_state          = (tracking_.channel[i].ch_tr_status & 0x00000001) |
                                                                       (tracking_.channel[i].ch_tr_status & 0x00000002) |
                                                                       (tracking_.channel[i].ch_tr_status & 0x00000004) |
                                                                       (tracking_.channel[i].ch_tr_status & 0x00000008) |
                                                                       (tracking_.channel[i].ch_tr_status & 0x00000010);

            tracking_.channel[i].tracking_status.channel_number      = ((tracking_.channel[i].ch_tr_status & 0x00000020)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x00000040)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x00000080)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x00000100)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x00000200)) >> 5;

            tracking_.channel[i].tracking_status.phase_lock          = ((tracking_.channel[i].ch_tr_status & 0x00000400) >> 10);
            tracking_.channel[i].tracking_status.parity_known        = ((tracking_.channel[i].ch_tr_status & 0x00000800) >> 11);
            tracking_.channel[i].tracking_status.code_lock           = ((tracking_.channel[i].ch_tr_status & 0x00001000) >> 12);

            tracking_.channel[i].tracking_status.correlator_type     = ((tracking_.channel[i].ch_tr_status & 0x00002000)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x00004000)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x00008000)) >> 13;

            tracking_.channel[i].tracking_status.satellite_system    = ((tracking_.channel[i].ch_tr_status & 0x00010000)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x00020000)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x00040000)) >> 16;

            tracking_.channel[i].tracking_status.grouping            = ((tracking_.channel[i].ch_tr_status & 0x00100000) >> 20);

            tracking_.channel[i].tracking_status.singal_type         = ((tracking_.channel[i].ch_tr_status & 0x00200000)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x00400000)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x00800000)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x01000000)  |
                                                                        (tracking_.channel[i].ch_tr_status & 0x02000000)) >> 21;

            tracking_.channel[i].tracking_status.fec                 = ((tracking_.channel[i].ch_tr_status & 0x04000000) >> 26);
            tracking_.channel[i].tracking_status.primary_l1          = ((tracking_.channel[i].ch_tr_status & 0x08000000) >> 27);
            tracking_.channel[i].tracking_status.half_cycle_added    = ((tracking_.channel[i].ch_tr_status & 0x10000000) >> 28);
            tracking_.channel[i].tracking_status.prn_lock            = ((tracking_.channel[i].ch_tr_status & 0x40000000) >> 30);
            tracking_.channel[i].tracking_status.channel_assignment  = ((tracking_.channel[i].ch_tr_status & 0x80000000) >> 31);
            // ROS_INFO("Satellite: %d", prn_);
            // ROS_INFO("with Pseudorange: %f", psr_);
            // ROS_INFO("with Doppler: %f", doppler_);
            // ROS_INFO("with Carrier to Noise Ratio: %f", CN0_);
        }
        // ROS_INFO("Solution Status = %d", solution_status_);
        // ROS_INFO("Position Type = %d", position_type_);
    }
    if(msg_id == RANGE)
    {
        memcpy(&pseudorange_.obs, &gps_data_[RANGE_OBS], sizeof(uint16_t));
        ROS_INFO("Number of observations: %d", pseudorange_.obs);
        pseudorange_.ranges.resize(pseudorange_.obs);

        for(int i = 0; i < pseudorange_.obs; ++i)
        {
            memcpy(&pseudorange_.ranges[i].prn_slot, &gps_data_[RANGE_PRN + i*RANGE_OFFSET], sizeof(uint16_t));

            memcpy(&pseudorange_.ranges[i].psr, &gps_data_[RANGE_PSR + i*RANGE_OFFSET], sizeof(double));
            memcpy(&pseudorange_.ranges[i].psr_std, &gps_data_[RANGE_PSR_STD + i*RANGE_OFFSET], sizeof(float));

            memcpy(&pseudorange_.ranges[i].adr, &gps_data_[RANGE_ADR + i*RANGE_OFFSET], sizeof(double));
            memcpy(&pseudorange_.ranges[i].adr_std, &gps_data_[RANGE_ADR_STD + i*RANGE_OFFSET], sizeof(float));

            memcpy(&pseudorange_.ranges[i].doppler, &gps_data_[RANGE_DOPPLER + i*RANGE_OFFSET], sizeof(float));

            memcpy(&pseudorange_.ranges[i].c_no, &gps_data_[RANGE_CNo + i*RANGE_OFFSET], sizeof(float));
            memcpy(&pseudorange_.ranges[i].locktime, &gps_data_[RANGE_LOCKTIME + i*RANGE_OFFSET], sizeof(float));
            memcpy(&pseudorange_.ranges[i].ch_tr_status, &gps_data_[RANGE_TRKSTART + i*RANGE_OFFSET], sizeof(uint32_t));

            pseudorange_.ranges[i].tracking_status.trck_state          = (pseudorange_.ranges[i].ch_tr_status & 0x00000001) |
                                                                         (pseudorange_.ranges[i].ch_tr_status & 0x00000002) |
                                                                         (pseudorange_.ranges[i].ch_tr_status & 0x00000004) |
                                                                         (pseudorange_.ranges[i].ch_tr_status & 0x00000008) |
                                                                         (pseudorange_.ranges[i].ch_tr_status & 0x00000010);

            pseudorange_.ranges[i].tracking_status.channel_number      = ((pseudorange_.ranges[i].ch_tr_status & 0x00000020)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x00000040)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x00000080)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x00000100)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x00000200)) >> 5;

            pseudorange_.ranges[i].tracking_status.phase_lock          = ((pseudorange_.ranges[i].ch_tr_status & 0x00000400) >> 10);
            pseudorange_.ranges[i].tracking_status.parity_known        = ((pseudorange_.ranges[i].ch_tr_status & 0x00000800) >> 11);
            pseudorange_.ranges[i].tracking_status.code_lock           = ((pseudorange_.ranges[i].ch_tr_status & 0x00001000) >> 12);

            pseudorange_.ranges[i].tracking_status.correlator_type     = ((pseudorange_.ranges[i].ch_tr_status & 0x00002000)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x00004000)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x00008000)) >> 13;

            pseudorange_.ranges[i].tracking_status.satellite_system    = ((pseudorange_.ranges[i].ch_tr_status & 0x00010000)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x00020000)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x00040000)) >> 16;

            pseudorange_.ranges[i].tracking_status.grouping            = ((pseudorange_.ranges[i].ch_tr_status & 0x00100000) >> 20);

            pseudorange_.ranges[i].tracking_status.singal_type         = ((pseudorange_.ranges[i].ch_tr_status & 0x00200000)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x00400000)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x00800000)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x01000000)  |
                                                                          (pseudorange_.ranges[i].ch_tr_status & 0x02000000)) >> 21;

            pseudorange_.ranges[i].tracking_status.fec                 = ((pseudorange_.ranges[i].ch_tr_status & 0x04000000) >> 26);
            pseudorange_.ranges[i].tracking_status.primary_l1          = ((pseudorange_.ranges[i].ch_tr_status & 0x08000000) >> 27);
            pseudorange_.ranges[i].tracking_status.half_cycle_added    = ((pseudorange_.ranges[i].ch_tr_status & 0x10000000) >> 28);
            pseudorange_.ranges[i].tracking_status.prn_lock            = ((pseudorange_.ranges[i].ch_tr_status & 0x40000000) >> 30);
            pseudorange_.ranges[i].tracking_status.channel_assignment  = ((pseudorange_.ranges[i].ch_tr_status & 0x80000000) >> 31);
        }
    }
}
/*
void GPS::print_formatted()
{
    ROS_INFO("GPS: p(%.3lf %.3lf %.3lf) sp(%.3lf %.3lf %.3lf) v(%.3lf %.3lf %.3lf) sv(%.3lf %.3lf %.3lf) status(%lx %lx)",
                longitude_,
                latitude_,
                altitude_,
                sigma_position_[0],
                sigma_position_[1],
                sigma_position_[2],
                velocity_[0],
                velocity_[1],
                velocity_[2],
                sigma_velocity_[0],
                sigma_velocity_[1],
                sigma_velocity_[2],
                (int32_t)position_status_,
                (int32_t)velocity_status_);
}
*/

void GPS::close()
{
    int err;

    if((err = serialcom_close(&gps_SerialPortConfig_)) != SERIALCOM_SUCCESS)
    {
        ROS_ERROR_STREAM("serialcom_close failed " << err);
        throwSerialComException(err);
    }
    ROS_INFO("gps closed.");
}

void GPS::configure()
{
    int err;

    // GPS should be configured to 9600 and change to 115200 during execution
    char buffer[100];
    // sprintf(buffer, "COM COM1,%d,N,8,1,N,OFF,ON", OLD_BPS);
    sprintf(buffer, "COM COM1,%d,N,8,1,N,OFF,ON", BPS);
    command(buffer);
    // command("COM COM2,115200,N,8,1,N,OFF,ON");

    if((err = serialcom_close(&gps_SerialPortConfig_)) != SERIALCOM_SUCCESS)
    {
        ROS_ERROR_STREAM("serialcom_close failed " << err);
        throwSerialComException(err);
    }

    // Reconnecting at 9600 bps
    if((err = serialcom_init(&gps_SerialPortConfig_, 1, (char*)serial_port_.c_str(), BPS)) != SERIALCOM_SUCCESS)
    {
        ROS_ERROR_STREAM("serialcom_init failed " << err);
        throwSerialComException(err);
    }

    // GPS time should be set approximately
    if(!getApproxTime())
    {
        ROS_ERROR("could not set approximate time");
    }
    else
    {
        sprintf(buffer, "SETAPPROXTIME %lu %f", gps_week_1024_, gps_secs_);
        command(buffer);
    }

    // GPS position should be set approximately (hard coded to LARA/UnB coordinates)
    command("SETAPPROXPOS -15.765824 -47.872109 1024");
    // command("SETAPPROXPOS -15.791372 -48.0227546 1178");
}

void GPS::receiveDataFromGPS(novatel_gps::LogAll* output_logall, novatel_gps::GpsXYZ *output_xyz)
{
    readDataFromReceiver();
    output_logall->msg_header = this->msg_header_;
    output_logall->range_log = pseudorange_;
    output_logall->sat_log = satellites_;
    output_logall->track_log = tracking_;

    output_xyz->position.position.x = x_;
    output_xyz->position.position.y = y_;
    output_xyz->position.position.z = z_;

    output_xyz->velocity.velocity.x = velocity_[0];
    output_xyz->velocity.velocity.y = velocity_[1];
    output_xyz->velocity.velocity.z = velocity_[2];

    output_xyz->position.covariance[0] = sigma_position_[0];
    output_xyz->position.covariance[4] = sigma_position_[1];
    output_xyz->position.covariance[8] = sigma_position_[2];

    output_xyz->velocity.covariance[0] = sigma_velocity_[0];
    output_xyz->velocity.covariance[4] = sigma_velocity_[1];
    output_xyz->velocity.covariance[8] = sigma_velocity_[2];
}

void GPS::receiveDataFromGPS(sensor_msgs::NavSatFix *output)
{
    readDataFromReceiver();
    output->latitude  = latitude_;
    output->longitude = longitude_;
    output->altitude  = altitude_;

    if(!solution_status_)
        output->status.status = sensor_msgs::NavSatStatus::STATUS_FIX;
    else
        output->status.status = sensor_msgs::NavSatStatus::STATUS_NO_FIX;

    output->position_covariance[0] = covar_latitude_;
    output->position_covariance[4] = covar_longitude_;
    output->position_covariance[8] = covar_altitude_;

    output->position_covariance_type = sensor_msgs::NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN;
}

void GPS::receiveDataFromGPS(novatel_gps::GpsXYZ *output)
{
    readDataFromReceiver();
    output->position.position.x = x_;
    output->position.position.y = y_;
    output->position.position.z = z_;

    output->velocity.velocity.x = velocity_[0];
    output->velocity.velocity.y = velocity_[1];
    output->velocity.velocity.z = velocity_[2];

    output->position.covariance[0] = sigma_position_[0];
    output->position.covariance[4] = sigma_position_[1];
    output->position.covariance[8] = sigma_position_[2];

    output->velocity.covariance[0] = sigma_velocity_[0];
    output->velocity.covariance[4] = sigma_velocity_[1];
    output->velocity.covariance[8] = sigma_velocity_[2];
}


void GPS::throwSerialComException(int err)
{
    switch(err)
    {
        case SERIALCOM_ERROR_IOPL:
        {
            throw std::runtime_error("serialcom error iopl");
            break;
        }
        case SERIALCOM_ERROR_MAXWAITENDOFTRANSMISSION:
        {
            throw std::runtime_error("serialcom error max wait end of transmission");
            break;
        }
        case SERIALCOM_ERROR_MAXWAITFORRECEPTION:
        {
            throw std::runtime_error("serialcom error max wait for reception");
            break;
        }
        case SERIALCOM_ERROR_MAXBPSPRECISION:
        {
            throw std::runtime_error("serialcom error max bps precision");
            break;
        }
        case SERIALCOM_ERROR_INCORRECTPORTNUMBER:
        {
            throw std::runtime_error("serialcom error incorrect port number");
            break;
        }
        case SERIALCOM_ERROR_INVALIDBAUDRATE:
        {
            throw std::runtime_error("serialcom error invalid baudrate");
            break;
        }
        case SERIALCOM_ERROR_INVALIDDEVICE:
        {
            throw std::runtime_error("serialcom error invalid device");
            break;
        }
    }
}

void GPS::command(const char* command)
{
    int i;
    int len = strlen(command);

    // Echo
    ROS_INFO("Sending command: %s", command);
    for(i = 0; i < len; i++)
    {
        serialcom_sendbyte(&gps_SerialPortConfig_, (unsigned char*) &command[i]);
        std::this_thread::sleep_for( std::chrono::milliseconds(5) );
    }

    // Sending Carriage Return character
    serialcom_sendbyte(&gps_SerialPortConfig_, (unsigned char*) "\r");
    std::this_thread::sleep_for( std::chrono::milliseconds(5) );

    // Sending Line Feed character
    serialcom_sendbyte(&gps_SerialPortConfig_, (unsigned char*) "\n");
    std::this_thread::sleep_for( std::chrono::milliseconds(5) );
}

// Calculate GPS week number and seconds, within 10 minutes of actual time, for initialization
int GPS::getApproxTime()
{
    // Time difference between January 1, 1970 and January 6, 1980
    // Source: http://www.timeanddate.com/date/durationresult.html?d1=1&m1=jan&y1=1970&h1=0&i1=0&s1=0&d2=6&m2=jan&y2=1980&h2=0&i2=0&s2=0
    const uint32_t time_diff = 315964800;

    // # of seconds in a week
    const uint32_t secs_in_week = 604800;

    // Unix time
    time_t cpu_secs;

    // Unprocessed GPS time
    uint32_t gps_time;

    // Get time
    cpu_secs = time(NULL);

    // Offset to GPS time and calculate weeks and seconds
    gps_time = cpu_secs - time_diff;
    gps_week_ = gps_time / secs_in_week;
    gps_week_1024_ = gps_week_ % 1024;
    gps_secs_ = gps_time % secs_in_week;

    if((gps_week_ != 0) && (gps_secs_ != 0))
        return 1;
    else
        return 0;
}
