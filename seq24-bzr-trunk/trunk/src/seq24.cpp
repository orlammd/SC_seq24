//----------------------------------------------------------------------------
//
//  This file is part of seq24.
//
//  seq24 is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  seq24 is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with seq24; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//-----------------------------------------------------------------------------

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __WIN32__
#    include "configwin32.h"
#else
#    include "config.h"
#endif

#include "font.h"
#ifdef LASH_SUPPORT
#    include "lash.h"
#endif
#include "mainwnd.h"
#include "midifile.h"
#include "optionsfile.h"
#include "perform.h"
#include "userfile.h"

/* struct for command parsing */
static struct
option long_options[] = {

    {"help", 0, 0, 'h'},
    {"showmidi", 0, 0, 's'},
    {"show_keys", 0, 0, 'k'},
    {"stats", 0, 0, 'S'},
    {"priority", 0, 0, 'p'},
    {"ignore", required_argument, 0, 'i'},
    {"interaction_method", required_argument, 0, 'x'},
    {"jack_transport",0, 0, 'j'},
    {"jack_master",0, 0, 'J'},
    {"jack_master_cond", 0, 0, 'C'},
    {"jack_start_mode", required_argument, 0, 'M'},
    {"jack_session_uuid", required_argument, 0, 'U'},
    {"manual_alsa_ports", 0, 0, 'm'},
    {"pass_sysex", 0, 0, 'P'},
    {"version", 0, 0, 'V'},
    {0, 0, 0, 0}

};

static const char versiontext[] = PACKAGE " " VERSION "\n";

bool global_manual_alsa_ports = false;
bool global_showmidi = false;
bool global_priority = false;
bool global_device_ignore = false;
int global_device_ignore_num = 0;
bool global_stats = false;
bool global_pass_sysex = false;
Glib::ustring global_filename = "";
Glib::ustring last_used_dir ="/";
std::string config_filename = ".seq24rc";
std::string user_filename = ".seq24usr";
bool global_print_keys = false;
interaction_method_e global_interactionmethod = e_seq24_interaction;

bool global_with_jack_transport = false;
bool global_with_jack_master = false;
bool global_with_jack_master_cond = false;
bool global_jack_start_mode = true;
Glib::ustring global_jack_session_uuid = "";

user_midi_bus_definition   global_user_midi_bus_definitions[c_maxBuses];
user_instrument_definition global_user_instrument_definitions[c_max_instruments];

font *p_font_renderer;

#ifdef LASH_SUPPORT
lash *lash_driver = NULL;
#endif

#ifdef __WIN32__
#   define HOME "HOMEPATH"
#   define SLASH "\\"
#else
#   define HOME "HOME"
#   define SLASH "/"
#endif


int
main (int argc, char *argv[])
{
    /* Scan the argument vector and strip off all parameters known to
     * GTK+. */
    Gtk::Main kit(argc, argv);

    /*prepare global MIDI definitions*/
    for ( int i=0; i<c_maxBuses; i++ )
    {
        for ( int j=0; j<16; j++ )
            global_user_midi_bus_definitions[i].instrument[j] = -1;
    }

    for ( int i=0; i<c_max_instruments; i++ )
    {
        for ( int j=0; j<128; j++ )
            global_user_instrument_definitions[i].controllers_active[j] = false;
    }


    /* Init the lash driver (strip lash specific command line
     * arguments and connect to daemon) */
#ifdef LASH_SUPPORT
    lash_driver = new lash(&argc, &argv);
#endif

    /* the main performance object */
    /* lash must be initialized here because mastermidibus uses the global
     * lash_driver variable*/
    perform p;

    /* read user preferences files */
    if ( getenv( HOME ) != NULL )
    {
        Glib::ustring home( getenv( HOME ));
        last_used_dir = home;
        Glib::ustring total_file = home + SLASH + config_filename;
        
        if (Glib::file_test(total_file, Glib::FILE_TEST_EXISTS))
        {
            printf( "Reading [%s]\n", total_file.c_str());

            optionsfile options( total_file );

            if ( !options.parse( &p ) ){
                printf( "Error Reading [%s]\n", total_file.c_str());
            }
        }

        total_file = home + SLASH + user_filename;
        if (Glib::file_test(total_file, Glib::FILE_TEST_EXISTS))
        {
            printf( "Reading [%s]\n", total_file.c_str());

            userfile user( total_file );

            if ( !user.parse( &p ) ){
                printf( "Error Reading [%s]\n", total_file.c_str());
            }
        }

    }
    else
        printf( "Error calling getenv( \"%s\" )\n", HOME );


    /* parse parameters */
    int c;

    while (true) {

        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long(argc, argv, "C:hi:jJmM:pPsSU:Vx:", long_options,
                &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c){

            case '?':
            case 'h':

                printf( "Usage: seq24 [OPTIONS] [FILENAME]\n\n" );
                printf( "Options:\n" );
                printf( "   -h, --help: show this message\n" );
                printf( "   -v, --version: show program version information\n" );
                printf( "   -m, --manual_alsa_ports: seq24 won't attach alsa ports\n" );
                printf( "   -s, --showmidi: dumps incoming midi events to screen\n" );
                printf( "   -p, --priority: runs higher priority with FIFO scheduler (must be root)\n" );
                printf( "   -P, --pass_sysex: passes any incoming sysex messages to all outputs \n" );
                printf( "   -i, --ignore <number>: ignore ALSA device\n" );
                printf( "   -k, --show_keys: prints pressed key value\n" );
                printf( "   -x, --interaction_method <number>: see .seq24rc for methods to use\n" );
                printf( "   -j, --jack_transport: seq24 will sync to jack transport\n" );
                printf( "   -J, --jack_master: seq24 will try to be jack master\n" );
                printf( "   -C, --jack_master_cond: jack master will fail if there is already a master\n" );
                printf( "   -M, --jack_start_mode <mode>: when seq24 is synced to jack, the following play\n" );
                printf( "                          modes are available (0 = live mode)\n");
                printf( "                                              (1 = song mode) (default)\n" );
                printf( "   -S, --stats: show statistics\n" );
                printf( "   -U, --jack_session_uuid <uuid>: set uuid for jack session\n" );
                printf( "\n\n\n" );

                return EXIT_SUCCESS;
                break;

            case 'S':
                global_stats = true;
                break;

            case 's':
                global_showmidi = true;
                break;

            case 'p':
                global_priority = true;
                break;

            case 'P':
                global_pass_sysex = true;
                break;

            case 'k':
                global_print_keys = true;
                break;

            case 'j':
                global_with_jack_transport = true;
                break;

            case 'J':
                global_with_jack_master = true;
                break;

            case 'C':
                global_with_jack_master_cond = true;
                break;

            case 'M':
                if (atoi( optarg ) > 0) {
                    global_jack_start_mode = true;
                }
                else {
                    global_jack_start_mode = false;
                }
                break;

            case 'm':
                global_manual_alsa_ports = true;
                break;

            case 'i':
                /* ignore alsa device */
                global_device_ignore = true;
                global_device_ignore_num = atoi( optarg );
                break;

            case 'V':
                printf("%s", versiontext);
                return EXIT_SUCCESS;
                break;

            case 'U':
                global_jack_session_uuid = Glib::ustring(optarg);
                break;

            case 'x':
                global_interactionmethod = (interaction_method_e)atoi(optarg);
                break;

            default:
                break;
        }

    } /* end while */


    p.init();
    p.launch_input_thread();
    p.launch_output_thread();
    p.init_jack();


    p_font_renderer = new font();

    mainwnd seq24_window( &p );
    if (optind < argc)
    {
        if (Glib::file_test(argv[optind], Glib::FILE_TEST_EXISTS))
            seq24_window.open_file(argv[optind]);
        else
            printf("File not found: %s\n", argv[optind]);
    }

    /* connect to lash daemon and poll events*/
#ifdef LASH_SUPPORT
    lash_driver->start(&p);
#endif
    kit.run(seq24_window);

    p.deinit_jack();

    if ( getenv( HOME ) != NULL )
    {
        string home( getenv( HOME ));
        Glib::ustring total_file = home + SLASH + config_filename;
        printf( "Writing [%s]\n", total_file.c_str());

        optionsfile options( total_file );

        if (!options.write( &p))
            printf( "Error writing [%s]\n", total_file.c_str());
    }
    else
    {
        printf( "Error calling getenv( \"%s\" )\n", HOME );
    }

#ifdef LASH_SUPPORT
    delete lash_driver;
#endif

    return EXIT_SUCCESS;
}

