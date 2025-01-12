/**
 * @file   simple_continuous_with_json.c
 * @author Jimi Damon <james.damon@accesio.com>
 * @date   Thu Nov 12 10:54:48 2015
 * @brief  Sample that demonstrates data ac
 *
 * 
 *
 * 
 * 
 */

#include <stdio.h>
#include <aiousb.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>

#include "AIOCountsConverter.h"
#include "AIOUSB_Log.h"
#include "aiocommon.h"
#include <getopt.h>
#include <signal.h>

struct channel_range *get_channel_range( char *optarg );
void process_cmd_line( struct opts *, int argc, char *argv[] );
void run_acquisition(AIOContinuousBuf *buf, AIOCommandLineOptions *options );


AIOUSB_BOOL fnd( AIOUSBDevice *dev ) { 
    if ( dev->ProductID >= USB_AI16_16A && dev->ProductID <= USB_AI12_128E ) { 
        return AIOUSB_TRUE;
    } else if ( dev->ProductID >=  USB_AIO16_16A && dev->ProductID <= USB_AIO12_128E ) {
        return AIOUSB_TRUE;
    } else {
        return AIOUSB_FALSE;
    }
}

FILE *fp;
AIOContinuousBuf *buf = 0;
#if defined(__clang__)
void sig_handler( int sig ) {
    printf("Forced exit, and will do so gracefully\n");
    AIOContinuousBufStopAcquisition(buf);
}
#endif 

int 
main(int argc, char *argv[] ) 
{
    AIOCommandLineOptions *options = NewDefaultAIOCommandLineOptions();
    struct sigaction sa;

    AIORET_TYPE retval = AIOUSB_SUCCESS;
    int *indices;
    int num_devices;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    /* Custom handler , catches INTR ( control-C ) and gracefully exits */
#if !defined(__clang__)
    sa.sa_handler = LAMBDA( void , (int sig) , { 
            printf("Forced exit, and will do so gracefully\n");
            AIOContinuousBufStopAcquisition(buf);
    });
#else
    sa.sa_handler = sig_handler;
    
#endif
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "Error with sigaction: \n");
        exit(1);
    }

    retval = AIOProcessCommandLine( options, &argc, argv );

    AIOUSB_Init();
    AIOUSB_ListDevices();



#if !defined(__clang__)
    /**
     * Alternatives are 
     * @verbatim
     * AIOUSB_FindDevices( &indices, &num_devices, find_ai_board );
     * AIOUSB_FindDevicesByGroup( &indices, &num_devices, AIO_ANALOG_INPUT() );
     * @endverbatim
     */
    AIOUSB_FindDevices( &indices, &num_devices, LAMBDA( AIOUSB_BOOL, (AIOUSBDevice *dev), {
                if ( dev->ProductID >= USB_AI16_16A && dev->ProductID <= USB_AI12_128E ) { 
                    return AIOUSB_TRUE;
                } else if ( dev->ProductID >=  USB_AIO16_16A && dev->ProductID <= USB_AIO12_128E ) {
                    return AIOUSB_TRUE;
                } else {
                    return AIOUSB_FALSE;
                }})
        );
#else
    AIOUSB_FindDevices( &indices, &num_devices, fnd );
#endif

    fp = fopen(options->outfile,"w");
    if( !fp ) {
        fprintf(stderr,"Unable to open '%s' for writing\n", options->outfile );
        exit(1);
    }

    if ( (retval = AIOCommandLineListDevices( options, indices, num_devices ) ) < AIOUSB_SUCCESS )
        exit(retval);

    
    /**
     *
     * @brief Start with the NewAIOContinousBufFromJSON( "{'aiocontin
     *
     *
     */

    if ( options->aiobuf_json ) { 
        if ( (buf = NewAIOContinuousBufFromJSON( options->aiobuf_json )) == NULL )
            exit(AIOUSB_ERROR_INVALID_AIOCONTINUOUS_BUFFER);
    } else if ( (buf = NewAIOContinuousBufFromJSON( options->default_aiobuf_json )) == NULL ) {
        exit(AIOUSB_ERROR_INVALID_AIOCONTINUOUS_BUFFER);
    }

    if ((retval = AIOContinuousBufOverrideAIOCommandLine( buf,options )) != AIOUSB_SUCCESS )
        exit(retval);

    fprintf(stderr,"Output: %s\n", AIOContinuousBufToJSON( buf ));

    for ( int i = 0; i < options->repeat ; i ++ ) { 
        AIOContinuousBufCallbackStart(buf);
        if ( getenv("PRE_SLEEP") ) { 
            printf("Sleeping for %d us \n", atoi(getenv("PRE_SLEEP")));
            usleep(atoi(getenv("PRE_SLEEP")));
        }
        run_acquisition( buf , options );
        
        if ( AIOContinuousBufGetStatus(buf) != TERMINATED ) { 
            fprintf(stderr,"Status was %d\n", (int)AIOContinuousBufGetStatus(buf));
        }

        AIOContinuousBufEnd(buf);           
    }

    AIOUSB_Exit();
    fclose(fp);
    fprintf(stderr,"Test completed...exiting\n");
    retval = ( retval >= 0 ? 0 : - retval );
    return(retval);
}

void run_acquisition(AIOContinuousBuf *buf, AIOCommandLineOptions *options )
{
    int64_t prevscans = 0;
    int tobufsize = (AIOContinuousBufGetNumberChannels(buf) * (AIOContinuousBufGetOversample(buf)+1))*2000;
    uint16_t *tobuf = (uint16_t *)malloc(sizeof(uint16_t)*tobufsize);

    while ( AIOContinuousBufPending(buf) ) { 
        int scans_available;

        if ( getenv("LOOP_SLEEP") ) { 
            usleep(atoi(getenv("LOOP_SLEEP")));
        }

        if ( ( scans_available = AIOContinuousBufCountScansAvailable(buf)) > 0 ) {

            int scans_read = AIOContinuousBufReadIntegerNumberOfScans( buf, tobuf, tobufsize, scans_available  );
            if ( scans_read >= 0 ) { 
                for ( int i = 0; i < scans_read; i ++ ) { 
                    for ( int j = 0; j < AIOContinuousBufGetNumberChannels(buf) ; j ++ )  {
                        int tmp = 0;
                        for ( int k = 0; k < (AIOContinuousBufGetOversample( buf )+1); k ++ ) { 
                            tmp += tobuf[i*AIOContinuousBufGetNumberChannels(buf)*(1+AIOContinuousBufGetOversample(buf))+j*(1+AIOContinuousBufGetOversample(buf))+k];
                        }
                        tmp /= (AIOContinuousBufGetOversample(buf)+1);
                        fprintf( fp, "%u,", tmp );
                    }
                    fprintf( fp, "\n");
                    if ( options->verbose && ( (buf->scans_read - prevscans) >  options->rate_limit  ) ) { 
                        fprintf(stdout,"Waiting : total=%ld, readpos=%d, writepos=%d\n", (long int)AIOContinuousBufGetScansRead(buf), 
                                (int)AIOContinuousBufGetReadPosition(buf), (int)AIOContinuousBufGetWritePosition(buf));
                        prevscans = buf->scans_read;
                    }
                }

            } else {
                fprintf(stderr,"Error reading scans : %d\n", scans_read );
            }
        }

        if ( getenv("STOP_EARLY") ) {
            int val = atoi(getenv("STOP_EARLY"));
            if ( val > 0 && AIOContinuousBufGetScansRead(buf) > val ) { 
                AIOContinuousBufEnd(buf);
            }
        }
    }
}
