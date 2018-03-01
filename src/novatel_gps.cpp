#include "novatel_gps.h"

/*************************** CRC functions (Firmware Reference Manual, p.32 + APN-030 Rev 1 Application Note) ***************************/

inline unsigned long ByteSwap (unsigned long n)
{ 
   return ( ((n &0x000000FF)<<24) + ((n &0x0000FF00)<<8) + ((n &0x00FF0000)>>8) + (( n &0xFF000000)>>24) ); 
}

#define CRC32_POLYNOMIAL    0xEDB88320L

/* --------------------------------------------------------------------------
Calculate a CRC value to be used by CRC calculation functions.
-------------------------------------------------------------------------- */
inline unsigned long CRC32Value(int i)
{
    int j;
    unsigned long ulCRC;
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
inline unsigned long CalculateBlockCRC32
(
    unsigned long ulCount,  /* Number of bytes in the data block */
    unsigned char *ucBuffer /* Data block */
)
{
    unsigned long ulTemp1;
    unsigned long ulTemp2;
    unsigned long ulCRC = 0;

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

GPS::GPS() : GPS_PACKET_SIZE(200)
{
    serial_port_ = "/dev/ttyUSB0";
    gps_week = 0;
    gps_secs = 0;

    D_SYNC0 = 0xAA;
    D_SYNC1 = 0x44;
    D_SYNC2 = 0x12;
    D_HDR_LEN = 28;

    // TODO: Where did these values come from?
    BXYZ_PSTAT    = (D_HDR_LEN);
    BXYZ_PTYPE    = (D_HDR_LEN+4);
    BXYZ_PX       = (D_HDR_LEN+8);
    BXYZ_PY       = (D_HDR_LEN+16);
    BXYZ_PZ       = (D_HDR_LEN+24);
    BXYZ_sPX      = (D_HDR_LEN+32);
    BXYZ_sPY      = (D_HDR_LEN+36);
    BXYZ_sPZ      = (D_HDR_LEN+40);
    BXYZ_VSTAT    = (D_HDR_LEN+44);
    BXYZ_VTYPE    = (D_HDR_LEN+48);
    BXYZ_VX       = (D_HDR_LEN+52);
    BXYZ_VY       = (D_HDR_LEN+60);
    BXYZ_VZ       = (D_HDR_LEN+68);
    BXYZ_sVX      = (D_HDR_LEN+76);
    BXYZ_sVY      = (D_HDR_LEN+80);
    BXYZ_sVZ      = (D_HDR_LEN+84);
    BXYZ_STNID    = (D_HDR_LEN+88);
    BXYZ_VLATE    = (D_HDR_LEN+92);
    BXYZ_DIFFAGE  = (D_HDR_LEN+96);
    BXYZ_SOLAGE   = (D_HDR_LEN+92);
    BXYZ_SV       = (D_HDR_LEN+92);
    BXYZ_SOLSV    = (D_HDR_LEN+92);

    BESTPOS_SOLSTAT     =  (D_HDR_LEN);
    BESTPOS_POSTYPE     =  (D_HDR_LEN+4);
    BESTPOS_LAT         =  (D_HDR_LEN+8);
    BESTPOS_LONG        =  (D_HDR_LEN+16);
    BESTPOS_HGT         =  (D_HDR_LEN+24);
    BESTPOS_UND         =  (D_HDR_LEN+32);
    BESTPOS_DATUMID     =  (D_HDR_LEN+36);
    BESTPOS_SLAT        =  (D_HDR_LEN+40);
    BESTPOS_SLON        =  (D_HDR_LEN+44);
    BESTPOS_SHGT        =  (D_HDR_LEN+48);
    BESTPOS_STNID       =  (D_HDR_LEN+52);
    BESTPOS_DIFFAGE     =  (D_HDR_LEN+56);
    BESTPOS_SOLAGE      =  (D_HDR_LEN+60);
    BESTPOS_SV          =  (D_HDR_LEN+64);
    BESTPOS_SOLNSV      =  (D_HDR_LEN+65);
    BESTPOS_GGL1        =  (D_HDR_LEN+66);
    BESTPOS_GGL1L2      =  (D_HDR_LEN+67);
    BESTPOS_EXTSOLSTAT  =  (D_HDR_LEN+69);
    BESTPOS_SIGMASK     =  (D_HDR_LEN+71);

    SATXYZ_NSAT         =  (D_HDR_LEN+8);
    SATXYZ_PRN          =  (D_HDR_LEN+12);
    SATXYZ_X            =  (D_HDR_LEN+16);
    SATXYZ_Y            =  (D_HDR_LEN+24);
    SATXYZ_Z            =  (D_HDR_LEN+32);
    SATXYZ_CLKCORR      =  (D_HDR_LEN+40);
    SATXYZ_IONCORR      =  (D_HDR_LEN+48);
    SATXYZ_TRPCORR      =  (D_HDR_LEN+56);

    TRACKSTAT_SOLSTAT   = (D_HDR_LEN);
    TRACKSTAT_POSTYPE   = (D_HDR_LEN+4);
    TRACKSTAT_CUTOFF    = (D_HDR_LEN+8);
    TRACKSTAT_CHAN      = (D_HDR_LEN+12);
    TRACKSTAT_PRN       = (D_HDR_LEN+16);
    TRACKSTAT_TRK_STAT  = (D_HDR_LEN+20);
    TRACKSTAT_PSR       = (D_HDR_LEN+24);
    TRACKSTAT_DOPPLER   = (D_HDR_LEN+32);
    TRACKSTAT_CN0       = (D_HDR_LEN+36);

    S_MSG_ID     = 2;
    S_MSG_LEN    = 2;
    S_SEQ_NUM    = 2;
    S_T_WEEK     = 2;
    S_T_MS       = 4;
    S_GPS_STATUS = 4;
    S_RESERVED   = 2;
    S_SW_VERS    = 2;
    S_CRC        = 4;

    TIMEOUT_US = 100000;
    OLD_BPS    = 9600;
    BPS        = 115200;
    MAX_BYTES  = 500;

    gps_data_.resize(GPS_PACKET_SIZE);
    velocity_.resize(3);
    sigma_position_.resize(3);
    sigma_velocity_.resize(3);

    latitude_  = 0.0;
    longitude_ = 0.0;
    altitude_  = 0.0;
    stdev_latitude_  = 0.;
    stdev_longitude_ = 0.;
    stdev_altitude_  = 0.;
    covar_latitude_  = 0.;
    covar_longitude_ = 0.;
    covar_altitude_  = 0.;
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
    if((err = serialcom_init(&gps_SerialPortConfig, 1, (char*)serial_port_.c_str(), OLD_BPS)) != SERIALCOM_SUCCESS)
    {
        ROS_ERROR_STREAM("serialcom_init failed " << err);
        throwSerialComException(err);
    }

    // Configure GPS, set baudrate to 115200 bps and reconnect
    configure();

    // Request GPS data
    if(log_id == BESTPOS)
        command("LOG BESTPOSB ONTIME 0.05");
    if(log_id == BESTXYZ)
        command("LOG BESTXYZB ONTIME 0.05");
    if(log_id == TRACKSTAT)
        command("LOG TRACKSTATB ONTIME 1");

    waitFirstFix();
}

void GPS::init(int log_id, std::string port)
{
    int err;

    waitReceiveInit();

    if(port != std::string())
        serial_port_ = port;

    // Init serial port at 9600 bps
    if((err = serialcom_init(&gps_SerialPortConfig, 1, (char*)serial_port_.c_str(), OLD_BPS)) != SERIALCOM_SUCCESS)
    {
        ROS_ERROR_STREAM("serialcom_init failed " << err);
        throwSerialComException(err);
    }

    // Configure GPS, set baudrate to 115200 bps and reconnect
    configure();

    // Request GPS data
    if(log_id == BESTPOS)
        command("LOG BESTPOSB ONTIME 0.05");
    if(log_id == BESTXYZ)
        command("LOG BESTXYZB ONTIME 0.05");
    if(log_id == TRACKSTAT)
        command("LOG TRACKSTATB ONTIME 1");

    waitFirstFix();
}

void GPS::waitReceiveInit()
{
    ROS_INFO("Wainting to Receiver initialize");
    ros::Duration(10.5).sleep();
}

void GPS::waitFirstFix()
{
    ROS_INFO("Wainting Time to First Fix");
    ros::Duration(45.0).sleep();

    readDataFromReceiver();

    if((position_status_ == 0)&&(velocity_status_ == 0))
    {
        ROS_INFO("Hot Start: First Fix Received");
        ROS_INFO("Streaming Nav Data");
    }
    // else
    // {
    //     ROS_WARN("Cold Start detected: First Fix not Received");
    //     ROS_INFO("Waiting Cold Time to First Fix");
    //     ros::Duration(15.0).sleep();
    // }
}


int GPS::readDataFromReceiver()
{
    int i;
    int err;
    int data_ready = 0;

    // State machine variables
    int b = 0, bb = 0, s = GPS_SYNC_ST;

    // Storage for data read from serial port
    unsigned char data_read;

    // Multi-byte data
    unsigned short msg_id, msg_len, t_week;
    unsigned long t_ms, crc_from_packet;

    //command("LOG BESTXYZB ONCE");

    // Try to sync with IMU and get latest data packet, up to MAX_BYTES read until failure
    for(i = 0; (!data_ready)&&(i < MAX_BYTES); i++)
    {
        // Read data from serial port
        if((err = serialcom_receivebyte(&gps_SerialPortConfig, &data_read, TIMEOUT_US)) != SERIALCOM_SUCCESS)
        {
                ROS_ERROR_STREAM("serialcom_receivebyte failed " << err);
                continue;
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
                            ROS_ERROR_STREAM("invalid HDR_LEN " << data_read);
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
                            memcpy((void*)&msg_id, (void*)&gps_data_[MSG_ID], sizeof(unsigned short));
                            
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
                            memcpy((void*)&msg_len, (void*)&gps_data_[MSG_LEN], sizeof(unsigned short));

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
                            //memcpy((void*)&seq_num, (void*)&gps_data_[SEQ_NUM], sizeof(unsigned short));
                            
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
                            memcpy((void*)&t_week, (void*)&gps_data_[T_WEEK], sizeof(unsigned short));
                            
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
                            memcpy((void*)&t_ms, (void*)&gps_data_[T_MS], sizeof(unsigned long));
                            
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
                            memcpy((void*)&(status_), (void*)&gps_data_[GPS_STATUS], sizeof(unsigned long));
                            
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
                            //memcpy((void*)&sw_vers, (void*)&gps_data_[SW_VERS], sizeof(unsigned short));
                            
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
                    unsigned long crc_calculated;
                    // Grab CRC from packet
                    crc_from_packet = ((unsigned long)((gps_data_[b+3] << 24) | (gps_data_[b+2] << 16) | (gps_data_[b+1] << 8) | gps_data_[b]));
                    
                    // Calculate CRC from packet (b = packet size)
                    // crc_calculated = CalculateBlockCRC32(b, &gps_data_[0]); // GAMBIARRA
                    crc_calculated = CalculateBlockCRC32(b, gps_data_.data()); // C++11
                    
                    // Compare them to see if valid packet 
                    if(0)
                    // if(crc_from_packet != ByteSwap(crc_calculated))
                    {
                    }
                    else
                    {
                        // ROS_ERROR("CRC does not match (%0lx != %0lx)", crc_from_packet, crc_calculated);
                        decode(msg_id);
                        data_ready = 1;
                    }
                        
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
    // tcflush(gps_SerialPortConfig.fd, TCIOFLUSH);
    // command("LOG BESTXYZB ONCE");
    return data_ready;
}

void GPS::decode(unsigned short msg_id)
{
    ROS_INFO("Message ID = %d", msg_id);
    if(sizeof(double) != 8)
        ROS_FATAL("sizeof(double) != 8, check decode");
    if(msg_id == BESTXYZ)
    {
        memcpy((void*)&position_status_, (void*)&gps_data_[BXYZ_PSTAT], sizeof(unsigned short));
        memcpy((void*)&position_type_, (void*)&gps_data_[BXYZ_PTYPE], sizeof(unsigned short));

        memcpy((void*)&x_, (void*)&gps_data_[BXYZ_PX], sizeof(double));
        memcpy((void*)&y_, (void*)&gps_data_[BXYZ_PY], sizeof(double));
        memcpy((void*)&z_, (void*)&gps_data_[BXYZ_PZ], sizeof(double));

        memcpy((void*)&sigma_position_[0], (void*)&gps_data_[BXYZ_sPX], sizeof(float));
        memcpy((void*)&sigma_position_[1], (void*)&gps_data_[BXYZ_sPY], sizeof(float));
        memcpy((void*)&sigma_position_[2], (void*)&gps_data_[BXYZ_sPZ], sizeof(float));

        memcpy((void*)&velocity_status_, (void*)&gps_data_[BXYZ_VSTAT], sizeof(unsigned short));
        memcpy((void*)&velocity_type_, (void*)&gps_data_[BXYZ_VTYPE], sizeof(unsigned short));

        memcpy((void*)&velocity_[0], (void*)&gps_data_[BXYZ_VX], sizeof(double));
        memcpy((void*)&velocity_[1], (void*)&gps_data_[BXYZ_VY], sizeof(double));
        memcpy((void*)&velocity_[2], (void*)&gps_data_[BXYZ_VZ], sizeof(double));

        memcpy((void*)&sigma_velocity_[0], (void*)&gps_data_[BXYZ_sVX], sizeof(float));
        memcpy((void*)&sigma_velocity_[1], (void*)&gps_data_[BXYZ_sVY], sizeof(float));
        memcpy((void*)&sigma_velocity_[2], (void*)&gps_data_[BXYZ_sVZ], sizeof(float));

        memcpy((void*)&number_sat_track_, (void*)&gps_data_[BXYZ_SV], sizeof(unsigned char));
        memcpy((void*)&number_sat_sol_, (void*)&gps_data_[BXYZ_SOLSV], sizeof(unsigned char));

        ROS_INFO("Position Solution Status = %d", position_status_);
        ROS_INFO("Velocity Solution Status = %d", velocity_status_);

        ROS_INFO("Sat = %d", number_sat_track_);
        ROS_INFO("Sat sol = %d", number_sat_sol_);
    }
    if(msg_id == BESTPOS)
    {
        memcpy((void*)&solution_status_, (void*)&gps_data_[BESTPOS_SOLSTAT], sizeof(unsigned short));
        memcpy((void*)&position_type_, (void*)&gps_data_[BESTPOS_POSTYPE], sizeof(unsigned short));

        memcpy((void*)&latitude_, (void*)&gps_data_[BESTPOS_LAT], sizeof(double));
        memcpy((void*)&longitude_, (void*)&gps_data_[BESTPOS_LONG], sizeof(double));
        memcpy((void*)&altitude_, (void*)&gps_data_[BESTPOS_HGT], sizeof(double));

        memcpy((void*)&stdev_latitude_, (void*)&gps_data_[BESTPOS_SLAT], sizeof(float));
        memcpy((void*)&stdev_longitude_, (void*)&gps_data_[BESTPOS_SLON], sizeof(float));
        memcpy((void*)&stdev_altitude_, (void*)&gps_data_[BESTPOS_SHGT], sizeof(float));

        memcpy((void*)&number_sat_track_, (void*)&gps_data_[BESTPOS_SV], sizeof(unsigned char));
        memcpy((void*)&number_sat_sol_, (void*)&gps_data_[BESTPOS_SOLNSV], sizeof(unsigned char));

        ROS_INFO("Sat = %d", number_sat_track_);
        ROS_INFO("Sat sol = %d", number_sat_sol_);
        covar_latitude_ = stdev_latitude_ * stdev_latitude_;
        covar_longitude_ = stdev_longitude_ * stdev_longitude_;
        covar_altitude_ = stdev_altitude_ * stdev_altitude_;
    }
    if(msg_id == SATXYZ)
    {
        memcpy((void*)&number_satellites_, (void*)&gps_data_[SATXYZ_NSAT], sizeof(unsigned long));
        memcpy((void*)&gps_prn_, (void*)&gps_data_[SATXYZ_PRN], sizeof(unsigned long));

        memcpy((void*)&x_, (void*)&gps_data_[SATXYZ_X], sizeof(double));
        memcpy((void*)&y_, (void*)&gps_data_[SATXYZ_Y], sizeof(double));
        memcpy((void*)&z_, (void*)&gps_data_[SATXYZ_Z], sizeof(double));

        memcpy((void*)&clk_correction_, (void*)&gps_data_[SATXYZ_CLKCORR], sizeof(double));
        memcpy((void*)&ion_correction_, (void*)&gps_data_[SATXYZ_IONCORR], sizeof(double));
        memcpy((void*)&trp_correction_, (void*)&gps_data_[SATXYZ_TRPCORR], sizeof(double));
    }
    if(msg_id == TRACKSTAT)
    {
        memcpy((void*)&solution_status_, (void*)&gps_data_[TRACKSTAT_SOLSTAT], sizeof(unsigned short));
        memcpy((void*)&position_type_, (void*)&gps_data_[TRACKSTAT_POSTYPE], sizeof(unsigned short));
        memcpy((void*)&cutoff_, (void*)&gps_data_[TRACKSTAT_CUTOFF], sizeof(float));
        memcpy((void*)&channels_, (void*)&gps_data_[TRACKSTAT_CHAN], sizeof(long));

        memcpy((void*)&prn_, (void*)&gps_data_[TRACKSTAT_PRN], sizeof(short));
        memcpy((void*)&trk_stat_, (void*)&gps_data_[TRACKSTAT_TRK_STAT], sizeof(unsigned long));
        memcpy((void*)&psr_, (void*)&gps_data_[TRACKSTAT_PSR], sizeof(double));
        memcpy((void*)&doppler_, (void*)&gps_data_[TRACKSTAT_DOPPLER], sizeof(float));
        memcpy((void*)&CN0_, (void*)&gps_data_[TRACKSTAT_CN0], sizeof(float));

        ROS_INFO("Solution Status = %d", solution_status_);
        ROS_INFO("Position Type = %d", position_type_);
        ROS_INFO("Channels = %ld", channels_);

        ROS_INFO("Satellite: %d", prn_);
        ROS_INFO("with Pseudorange: %f", psr_);
        ROS_INFO("with Doppler: %f", doppler_);
        ROS_INFO("with Carrier to Noise Ratio: %f", CN0_);
    }
}

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
                (long)position_status_,
                (long)velocity_status_);
}

void GPS::close()
{
    int err;

    if((err = serialcom_close(&gps_SerialPortConfig)) != SERIALCOM_SUCCESS)
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
    command("COM COM1,115200,N,8,1,N,OFF,ON");
    // command("COM COM2,115200,N,8,1,N,OFF,ON");
    
    if((err = serialcom_close(&gps_SerialPortConfig)) != SERIALCOM_SUCCESS)
    {
        ROS_ERROR_STREAM("serialcom_close failed " << err);
        throwSerialComException(err);
    }

    // Reconnecting at 115200 bps
    if((err = serialcom_init(&gps_SerialPortConfig, 1, (char*)serial_port_.c_str(), BPS)) != SERIALCOM_SUCCESS)
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
        char buf[100];
        sprintf(buf, "SETAPPROXTIME %lu %lu", gps_week, gps_secs);
        command(buf);
    }

    // GPS position should be set approximately (hard coded to LARA/UnB coordinates)
    command("SETAPPROXPOS -15.765824 -47.872109 1024");
    // command("SETAPPROXPOS -15.791372 -48.0227546 1178");
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
        serialcom_sendbyte(&gps_SerialPortConfig, (unsigned char*) &command[i]);
        usleep(5000);
    }

    serialcom_sendbyte(&gps_SerialPortConfig, (unsigned char*) "\r");
    usleep(5000);
    serialcom_sendbyte(&gps_SerialPortConfig, (unsigned char*) "\n");
    usleep(5000);
}

// Calculate GPS week number and seconds, within 10 minutes of actual time, for initialization
int GPS::getApproxTime()
{
    // Time difference between January 1, 1970 and January 6, 1980
    // Source: http://www.timeanddate.com/date/durationresult.html?d1=1&m1=jan&y1=1970&h1=0&i1=0&s1=0&d2=6&m2=jan&y2=1980&h2=0&i2=0&s2=0
    const unsigned long int time_diff = 315964800;

    // # of seconds in a week
    const unsigned long int secs_in_week = 604800;

    // Unix time
    time_t cpu_secs;

    // Unprocessed GPS time
    unsigned long int gps_time;

    // Get time
    cpu_secs = time(NULL);

    // Offset to GPS time and calculate weeks and seconds
    gps_time = cpu_secs - time_diff;
    gps_week = gps_time / secs_in_week;
    gps_secs = gps_time % secs_in_week;

    if((gps_week != 0) && (gps_secs != 0))
        return 1;
    else
        return 0;
}

void GPS::gtime(unsigned char t_status, unsigned short t_week, unsigned long t_ms)
{
    // GPS week number = full week number starting from midnight of the night from January 5 to January 6, 1980
    // TO DO: figure out what this was for. I think it was supposed to decode time information and know if GPS has valid time
}

