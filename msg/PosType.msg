# 0 NONE No solution
# 1 FIXEDPOS Position has been fixed by the FIX POSITION command
# 2 FIXEDHEIGHT Position has been fixed by the FIX HEIGHT/AUTO command
# 8 DOPPLER_VELOCITY Velocity computed using instantaneous Doppler
# 16 SINGLE Single point position
# 17 PSRDIFF Pseudorange differential solution
# 18 WAAS Solution calculated using corrections from an SBAS
# 19 PROPAGATED Propagated by a Kalman filter without new observations
# 20 OMNISTAR a OmniSTAR VBS position (L1 sub-metre)
# 32 L1_FLOAT Floating L1 ambiguity solution
# 33 IONOFREE_FLOAT Floating ionospheric-free ambiguity solution
# 34 NARROW_FLOAT Floating narrow-lane ambiguity solution
# 48 L1_INT Integer L1 ambiguity solution
# 49 WIDE_INT Integer wide-lane ambiguity solution
# 50 NARROW_INT Integer narrow-lane ambiguity solution
# 51 RTK_DIRECT_INS b RTK status where the RTK filter is directly initialized from the INS filter
# 52 INS b INS calculated position corrected for the antenna
# 53 INS_PSRSP b INS pseudorange single point solution - no DGPS corrections
# 54 INS_PSRDIFF b INS pseudorange differential solution
# 55 INS_RTKFLOAT b INS RTK floating point ambiguities solution
# 56 INS_RTKFIXED b INS RTK fixed ambiguities solution
# 64 OMNISTAR_HP a OmniSTAR HP position
# 65 OMNISTAR_XP a OmniSTAR XP position
# 66 CDGPS a Position solution using CDGPS correction


uint8 NONE              = 0
uint8 FIXEDPOS          = 1
uint8 FIXEDHEIGHT       = 2
uint8 DOPPLER_VELOCITY  = 8
uint8 SINGLE            = 16
uint8 PSRDIFF           = 17
uint8 WAAS              = 18
uint8 PROPAGATED        = 19
uint8 OMNISTAR          = 20
uint8 L1_FLOAT          = 32
uint8 IONOFREE_FLOAT    = 33
uint8 NARROW_FLOAT      = 34
uint8 L1_INT            = 48
uint8 WIDE_INT          = 49
uint8 NARROW_INT        = 50
uint8 RTK_DIRECT_INS    = 51
uint8 INS               = 52
uint8 INS_PSRSP         = 53
uint8 INS_PSRDIFF       = 54
uint8 INS_RTKFLOAT      = 55
uint8 INS_RTKFIXED      = 56
uint8 OMNISTAR_HP       = 64
uint8 OMNISTAR_XP       = 65
uint8 CDGPS             = 66

uint8 position_type
