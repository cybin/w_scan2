/*
 * Simple MPEG/DVB parser to achieve network/service information without initial tuning data
 *
 * Copyright (C) 2006-2014 Winfried Koehler
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "si_types.h"
#include "dump-xml.h"

typedef struct {
	char name[32];
	uint8_t delsys[32];
	uint16_t xml_default;
} xml_param;

#define NO_AUTO (1<<15)

const xml_param xml_params[] = {
	// terrestrial_delivery_system_descriptor
	{"bandwidth", {SYS_DVBT, SYS_DVBT2, 0}, NO_AUTO},
	{"priority", {SYS_DVBT, SYS_DVBT2, 0}, true},
	{"time_slicing", {SYS_DVBT, 0}, false},	// EN 301 192
	{"mpe_fec", {SYS_DVBT, SYS_DVBT2, 0}, false},
	{"modulation", {SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBC_ANNEX_C,
			SYS_DVBT, SYS_DVBT2,
			SYS_DVBS, SYS_DVBS2, SYS_DSS, SYS_TURBO,
			SYS_DVBH,
			SYS_ISDBT, SYS_ISDBS, SYS_ISDBC,
			SYS_ATSC, SYS_ATSCMH,
			SYS_DTMB, SYS_CMMB, SYS_DAB, 0}, QAM_AUTO},
	{"hierarchy", {SYS_DVBT, SYS_DVBT2, 0}, HIERARCHY_NONE},
	{"alpha", {SYS_DVBT, SYS_DVBT2, 0}, NO_AUTO},	//Table 46: Signalling format for the avalues and the used interleaver
	{"interleaver", {SYS_DVBT, SYS_DVBT2, 0}, INTERLEAVE_AUTO},	//Table 46: Signalling format for the avalues and the used interleaver
	{"coderate", {SYS_DVBT, SYS_DVBT2,
		      SYS_DVBS, SYS_DVBS2, SYS_DSS, SYS_TURBO,
		      SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBC_ANNEX_C,
		      SYS_DTMB, 0}, FEC_AUTO},
	{"coderate_LP", {SYS_DVBT, SYS_DVBT2, 0}, FEC_NONE},
	{"guard", {SYS_DVBT, SYS_DVBT2, SYS_DTMB, 0}, GUARD_INTERVAL_AUTO},
	{"transmission", {SYS_DVBT, SYS_DVBT2, SYS_DTMB, 0},
	 TRANSMISSION_MODE_AUTO},
	{"other_frequency_flag", {SYS_DVBT, SYS_DVBT2, 0}, 0},
	// T2_delivery_system_descriptor
	{"plp_id", {SYS_DVBT2, SYS_DVBC2, 0}, 0},
	{"system_id", {SYS_DVBT2, SYS_DVBC2, 0}, 0},
	{"extended_info", {SYS_DVBT2, 0}, 1},	// T2 delivery system descriptor: 'if (descriptor_length > 4){'
	{"siso_miso", {SYS_DVBT2, 0}, 0},
	{"tfs_flag", {SYS_DVBT2, 0}, 0},

	// satellite_delivery_system_descriptor
	{"orbital_position", {SYS_DVBS, SYS_DVBS2, SYS_DSS, SYS_TURBO, 0},
	 0x0192},
	{"west_east_flag", {SYS_DVBS, SYS_DVBS2, SYS_DSS, SYS_TURBO, 0}, 'E'},
	{"polarization", {SYS_DVBS, SYS_DVBS2, SYS_DSS, SYS_TURBO, 0}, NO_AUTO},
	{"rolloff", {SYS_DVBS2, 0}, ROLLOFF_AUTO},
	{"symbolrate", {SYS_DVBS, SYS_DVBS2, SYS_DSS, SYS_TURBO,
			SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_DVBC_ANNEX_C,
			0},
	 NO_AUTO},

	// see S2_satellite_delivery_system_descriptor
	{"multiple_input_stream_flag", {SYS_DVBS2, 0}, 0},
	{"scrambling_sequence_selector", {SYS_DVBS2, 0}, 0},
	{"scrambling_sequence_index", {SYS_DVBS2, 0}, 0},
	{"input_stream_id", {SYS_DVBS2, 0}, NO_AUTO},
	{"pilot", {SYS_DVBS2, 0}, PILOT_AUTO},

	//
	{"interleave", {SYS_DTMB, 0}, INTERLEAVING_AUTO},

	// C2_delivery_system_descriptor
	{"data_slice_id", {SYS_DVBC2, 0}, 0},
	{"C2_System_tuning_frequency_type", {SYS_DVBC2, 0},
	 C2_SYSTEM_CENTER_FREQUENCY},
	{"active_OFDM_symbol_duration", {SYS_DVBC2, 0}, FFT_4K_8MHZ},
	{"", {0}, NO_AUTO},
};

#define NUM_XML_PARAMS (sizeof(xml_params) / sizeof(xml_params[0]))

static bool want_to_print(const char *param, uint8_t delsys, uint16_t value)
{
	uint16_t i;
	uint8_t j;
	bool found_param = false;

	for (i = 0; i < NUM_XML_PARAMS - 1; i++) {
		if (strcmp(xml_params[i].name, param))
			continue;
		found_param = true;
		for (j = 0; j < 32; j++) {
			if (xml_params[i].delsys[j] == 0)
				break;
			if (xml_params[i].delsys[j] == delsys) {
				if (value != xml_params[i].xml_default)
					return true;
				else
					return false;
			}
		}
	}

	if (!found_param)
		warning("could not find \"%s\" in list of xml_params\n", param);
	return false;
}

/*
  fe_inversion_t             inversion;
  uint32_t                   symbolrate;                     // symbols per second                                  6..9  //
  uint32_t                   bandwidth;                      // Hz                                                  10..13//
  uint16_t                   orbital_position;               // 0x0000-0x1800                                       14..15//
  uint8_t                    input_stream_identifier;        // ISI 8bit.                                           16    //
  fe_delivery_system_t       delsys                      :6; //                               0..18 -> 5 -> 6 (63)  17    //
  fe_polarization_t          polarization                :2; // H,V,L,R                       0..3  -> 2 -> 2 (3)   17    //
  fe_modulation_t            modulation                  :6; //                               0..13 -> 4 -> 6 (63)  18    //
  fe_pilot_t                 pilot                       :2; // pilot, on, off, auto          0..2  -> 2 -> 2 (3)   18    //
  fe_code_rate_t             coderate                    :7; // code rate                     0..12 -> 4 -> 7 (127) 19    //
  fe_code_rate_t             coderate_LP                 :7; // code rate low priority stream 0..12 -> 4 -> 7 (127) 19..20//
  fe_guard_interval_t        guard_interval              :7; //                               0..10 -> 4 -> 7 (127) 20..21//
  fe_rolloff_t               rolloff                     :3; // 0.35, 0.25, 0.20, auto        0..3  -> 2 -> 3 (7)   21    //
  fe_transmit_mode_t         transmission_mode           :7; //                               0..8  -> 4 -> 7 (127) 22    //
  fe_west_east_flag_t        west_east_flag              :1; // east, west                    0..1  -> 1 -> 1 (1)   22    //
  fe_hierarchy_t             hierarchy                   :4; //                               0..4  -> 3 -> 4 (15)  23    //
  fe_time_slicing_t          time_slicing                :2; // only defined in w_scan2.       0..2  -> 2 -> 2 (3)   23    //
  unsigned                   scrambling_sequence_index   :18;// not yet in use.                                     23..25//
  unsigned                   scrambling_sequence_selector:1; // on/off                        0..1  -> 1            26    //
  unsigned                   multiple_input_stream_flag  :1; // on/off                        0..1  -> 1            26    //
  fe_frequency_type_t        C2_tuning_frequency_type    :2; // 0..2                          0..2  -> 2 -> 2 (3)   26    //
  fe_ofdm_symbol_duration_t  active_OFDM_symbol_duration :2; //                                                     26    //
  fe_alpha_t                 alpha                       :2; // only defined in w_scan2                              26    //
  fe_interleave_t            interleaver                 :2; // only defined in w_scan2                              27    //
  fe_priority_t              priority                    :2; // only defined in w_scan2                              27    //
  fe_mpe_fec_t               mpe_fec                     :2; // only defined in w_scan2                              27    //
  unsigned                   extended_info               :1; //                                                     27    //
  unsigned                   SISO_MISO                   :1; // 1 = multiple input single output                    27    //
  unsigned                   tfs_flag                    :1; // 1 if Time-Frequency-Slicing                         28    //
  unsigned                   other_frequency_flag        :1; // DVB-T/T2                                            28    //
  unsigned                   last_tuning_failed          :1; //                                                     28    //
  unsigned                   type                        :5; // 3bit + 2bit byte_align                              28    //
  uint32_t                   source;                         //                                                     29..32//
  uint16_t                   system_id;                      // DVB-C2, DVB-T2 system_id                            33..34//
  uint8_t                    plp_id;                         // DVB-C2, DVB-T2                                      35    //
  uint8_t                    data_slice_id;                  // DVB-C2                                              36    //
*/

#define needs_param(p) (want_to_print(#p, t->delsys, t->p))

static const char *get_indent(int indent)
{
	static char buf[256];
	memset(&buf[0], ' ', 255);
	buf[255] = 0;
	buf[3 * indent] = 0;
	return &buf[0];
}

void xml_dump(FILE * dest, pList transponders)
{
	struct transponder *t;
	int indent = 0;

	fprintf(dest, "<?xml version=\"1.0\" ?>\n");
	fprintf(dest,
		"<!DOCTYPE service_list SYSTEM \"https://raw.githubusercontent.com/stefantalpalaru/w_scan2/master/doc/service_list.dtd\">\n");
	fprintf(dest, "\n");
	fprintf(dest, "<!-- NOTE:\n");
	fprintf(dest, "     if reading or writing w_scan2 XML file format:\n");
	fprintf(dest, "        - please validate XML against DTD above.\n");
	fprintf(dest, "        - indent each XML element\n");
	fprintf(dest,
		"        - indent using three spaces, dont use <TAB> char to indent.\n");
	fprintf(dest,
		"        - conform to requirements mentionend in DTD file.\n");
	fprintf(dest, " -->\n\n");
	fprintf(dest, "%s<service_list>\n", get_indent(indent));

	indent++;
	fprintf(dest, "%s<transponders>\n", get_indent(indent));
	for (t = transponders->first; t; t = t->next) {
		indent++;
		fprintf(dest,
			"%s<transponder ONID=\"%d\" NID=\"%d\" TSID=\"%d\">\n",
			get_indent(indent), t->original_network_id,
			t->network_id, t->transport_stream_id);
		indent++;
		fprintf(dest,
			"%s<params delsys=\"%s\" center_frequency=\"%.3f\">\n",
			get_indent(indent), delivery_system_name(t->delsys),
			(double)t->frequency / 1e6);
		indent++;

		switch (t->delsys) {
		case SYS_DVBT:
		case SYS_DVBT2:
			{
				if needs_param
					(modulation)
					    fprintf(dest,
						    "%s<param modulation=\"%s\"/>\n",
						    get_indent(indent),
						    modulation_name
						    (t->modulation));
				if needs_param
					(bandwidth)
					    fprintf(dest,
						    "%s<param bandwidth=\"%.3f\"/>\n",
						    get_indent(indent),
						    (double)t->bandwidth / 1e6);
				if needs_param
					(coderate)
					    fprintf(dest,
						    "%s<param coderate=\"%s\"/>\n",
						    get_indent(indent),
						    coderate_name(t->coderate));
				if needs_param
					(transmission)
					    fprintf(dest,
						    "%s<param transmission=\"%s\"/>\n",
						    get_indent(indent),
						    transmission_mode_name
						    (t->transmission));
				if needs_param
					(guard)
					    fprintf(dest,
						    "%s<param guard=\"%s\"/>\n",
						    get_indent(indent),
						    guard_interval_name
						    (t->guard));
				if (t->hierarchy != HIERARCHY_NONE) {
					// print those only if hierarchy is used.
					if needs_param
						(hierarchy)
						    fprintf(dest,
							    "%s<param hierarchy=\"%s\"/>\n",
							    get_indent(indent),
							    hierarchy_name
							    (t->hierarchy));
					if needs_param
						(alpha)
						    fprintf(dest,
							    "%s<param alpha=\"%s\"/>\n",
							    get_indent(indent),
							    alpha_name
							    (t->alpha));
					if needs_param
						(terr_interleaver)
						    fprintf(dest,
							    "%s<param terr_interleaver=\"%s\"/>\n",
							    get_indent(indent),
							    interleaver_name
							    (t->
							     terr_interleaver));
					if needs_param
						(coderate_LP)
						    fprintf(dest,
							    "%s<param coderate_LP=\"%s\"/>\n",
							    get_indent(indent),
							    coderate_name
							    (t->coderate_LP));
					if needs_param
						(priority)
						    fprintf(dest,
							    "%s<param priority=\"%s\"/>\n",
							    get_indent(indent),
							    bool_name
							    (t->priority));
				}
				if needs_param
					(mpe_fec)
					    fprintf(dest,
						    "%s<param mpe_fec=\"%s\"/>\n",
						    get_indent(indent),
						    bool_name(t->mpe_fec));
				if needs_param
					(time_slicing)
					    fprintf(dest,
						    "%s<param time_slicing=\"%s\"/>\n",
						    get_indent(indent),
						    bool_name(t->time_slicing));
				if needs_param
					(system_id)
					    fprintf(dest,
						    "%s<param system_id=\"%d\"/>\n",
						    get_indent(indent),
						    t->system_id);
				if needs_param
					(plp_id)
					    fprintf(dest,
						    "%s<param plp_id=\"%d\"/>\n",
						    get_indent(indent),
						    t->plp_id);
				if ((t->other_frequency_flag != false)
				    && ((t->cells)->count > 0)) {
					struct cell *f;
					if needs_param
						(other_frequency_flag) {
						fprintf(dest,
							"%s<param other_frequency_flag=\"%s\"/>\n",
							get_indent(indent),
							bool_name(true));
						fprintf(dest,
							"%s<frequency_list>\n",
							get_indent(indent));
						indent++;
						for (f = t->cells->first; f;
						     f = f->next) {
							if (t->tfs_flag) {
								fprintf(dest,
									"%s<tfs_center>\n",
									get_indent
									(indent));

							}
						}

						indent--;
						fprintf(dest,
							"%s</frequency_list>\n",
							get_indent(indent));
						}
				}
				break;
			}
		default:
			fatal
			    ("unimplemented delivery system \"%s\"for %s XML output\n",
			     delivery_system_name(t->delsys), PACKAGE_NAME);

		}

		indent--;
		fprintf(dest, "%s</params>\n", get_indent(indent));
		indent--;
		fprintf(dest, "%s</transponder>\n", get_indent(indent));
		indent--;
	}
	fprintf(dest, "%s</transponders>\n", get_indent(indent));
	indent--;

	fprintf(dest, "%s<service_list>\n", get_indent(indent));

/*

<service_list>
   <transponders>
      <transponder ONID="8438" NID="13100" TSID="13057">
         <params delsys="SYS_DVBT2" center_frequency="177.500">
            <param modulation="QAM_AUTO"/>
            <param bandwidth="8.000"/>
            <param coderate="FEC_AUTO"/>
            <param transmission="TRANSMISSION_MODE_32K"/>
            <param guard="GUARD_INTERVAL_19_256"/>
            <param hierarchy="HIERARCHY_NONE"/>
            <param plp_id="0"/>
         </params>
      </transponder>
   </transponders>

   <services>
      <service ONID="8438" TSID="13057" SID="102">
         <name char256="Nelonen Pro 2 HD"/>
         <provider char256="DNA"/>
         <pcr pid="213"/>
         <streams>
            <stream type="27" pid="213" description="AVC Video stream, ITU-T Rec. H.264 | ISO/IEC 14496-10"/>
            <stream type="4" pid="330" description="AUDIO" language_code="fin"/>
         </streams>
         <CA_systems>
            <CA_system name="Conax" ca_id="0x0B00"/>
            <CA_system name="Conax" ca_id="0x0B02"/>
         </CA_systems>
         <comment char256="Nelonen Pro 2 HD is twice on 177.5MHz"/>
      </service>
   </services>
</service_list>
*/

}


void xml_dump_transponders(FILE* dest, pList transponders) {

  struct transponder * t;
  int indent = 1;

  fprintf(dest, "%s<transponders>\n", get_indent(indent));  

  for(t = transponders->first; t; t = t->next) {
		indent++;
		fprintf(dest, "%s<transponder ONID=\"%d\" NID=\"%d\" TSID=\"%d\">\n",
					get_indent(indent),
					t->original_network_id,
					t->network_id,
					t->transport_stream_id
				);
		indent++;
		fprintf(dest, "%s<delivery_system>%s</delivery_system>\n", get_indent(indent), delivery_system_name(t->delsys));
		fprintf(dest, "%s<frequency>%.3f</frequency>\n", get_indent(indent), (double) t->frequency/1e6);
	 // fprintf(dest, "%s<params delsys=\"%s\" center_frequency=\"%.3f\">\n",
	 //		get_indent(indent), delivery_system_name(t->delsys), (double) t->frequency/1e6);
		//indent++;
	
		switch(t->delsys) {
			case SYS_DVBT:
			case SYS_DVBT2:
			{
			//		   if needs_param(modulation)
				fprintf(dest, "%s<modulation>%s</modulation>\n", get_indent(indent), modulation_name(t->modulation));
				
				if needs_param(bandwidth)
					fprintf(dest, "%s<bandwidth>%.3f</bandwidth>\n", get_indent(indent), (double) t->bandwidth/1e6);  
				if needs_param(coderate)
					fprintf(dest, "%s<coderate>%s</coderate>\n", get_indent(indent), coderate_name(t->coderate)); 
				if needs_param(symbolrate)
					fprintf(dest, "%s<symbolrate>%ui</symbolrate>\n", get_indent(indent), t->symbolrate); 
				if needs_param(transmission)
					fprintf(dest, "%s<transmission>%s</transmission>\n", get_indent(indent), transmission_mode_name(t->transmission)); 
				if needs_param(guard)
					fprintf(dest, "%s<guard>%s</guard>\n", get_indent(indent), guard_interval_name(t->guard)); 
				if (t->hierarchy != HIERARCHY_NONE) {
               // print those only if hierarchy is used.
               if needs_param(hierarchy)
               fprintf(dest, "%s<hierarchy>%s</hierarchy>\n", get_indent(indent), hierarchy_name(t->hierarchy));
               
               if needs_param(alpha)
                  fprintf(dest, "%s<alpha>%s</alpha>\n", get_indent(indent), alpha_name(t->alpha));
               if needs_param(terr_interleaver)
                  fprintf(dest, "%s<terr_interleaver>%s</terr_interleaver>\n", get_indent(indent), interleaver_name(t->terr_interleaver));
               if needs_param(coderate_LP)
                  fprintf(dest, "%s<coderate_LP>%s</coderate_LP>\n", get_indent(indent), coderate_name(t->coderate_LP));
               if needs_param(priority)
                  fprintf(dest, "%s<priority>%s</priority>\n", get_indent(indent), bool_name(t->priority));
				}
				if needs_param(mpe_fec)
					fprintf(dest, "%s<mpe_fec>%s</mpe_fec>\n", get_indent(indent), bool_name(t->mpe_fec));
				if needs_param(time_slicing)
					fprintf(dest, "%s<time_slicing>%s</time_slicing>\n", get_indent(indent), bool_name(t->time_slicing)); 
				if needs_param(system_id)
					fprintf(dest, "%s<system_id>%d</system_id>\n", get_indent(indent), t->system_id);
				if needs_param(plp_id)
					fprintf(dest, "%s<plp_id>%d</plp_id>\n", get_indent(indent), t->plp_id); 
	 
				if ((t->other_frequency_flag != false) && ((t->cells)->count > 0)) {
					struct cell* f;
					
					if needs_param(other_frequency_flag) {
						fprintf(dest, "%s<other_frequency_flag>%s</other_frequency_flag>\n", get_indent(indent), bool_name(true));
						fprintf(dest, "%s<frequency_list>\n", get_indent(indent));
						indent++;
						for(f = t->cells->first; f; f = f->next) {
							
							if (t->tfs_flag) {
								 fprintf(dest, "%s<tfs_center>\n", get_indent(indent));
							}
							else {
								 //for(g = f->transposers->first; g; g = g->next) {
								 //   }
							}
						}
		
						indent--;
						fprintf(dest, "%s</frequency_list>\n", get_indent(indent));
					}
				}
				break;
			}
			case SYS_DVBS2:
			case SYS_DVBS:
			{
				char pol = 0;
 //		   if needs_param(modulation)
				fprintf(dest, "%s<modulation>%s</modulation>\n", get_indent(indent), modulation_name(t->modulation));            
				if needs_param(bandwidth)
					fprintf(dest, "%s<bandwidth>%.3f</bandwidth>\n", get_indent(indent), (double) t->bandwidth/1e6);  
				//if needs_param(coderate)
				fprintf(dest, "%s<coderate>%s</coderate>\n", get_indent(indent), coderate_name(t->coderate)); 
	//		   if needs_param(symbolrate)
				fprintf(dest, "%s<symbolrate>%u</symbolrate>\n", get_indent(indent), t->symbolrate/1000); 
	//		   if needs_param(polarization) {
			 
				switch (t->polarization) {
					case POLARIZATION_HORIZONTAL:
						pol = 'H';
						break;
					case POLARIZATION_VERTICAL:
						pol = 'V';
						break;
					default:
						break;
			 }
			 
			 fprintf(dest, "%s<polarization>%c</polarization>\n", get_indent(indent), pol); 
	//		   }
				break;
			}
				 
			default:
				//fatal("unimplemented delivery system \"%s\"for w_scan XML output\n",
				//	 delivery_system_name(t->delsys));
				error("unimplemented delivery system for w_scan XML output\n");
		} 
	
	
	
		indent--;     
		fprintf(dest, "%s</transponder>\n", get_indent(indent));
		indent--;
	}

  fprintf(dest, "%s</transponders>\n", get_indent(indent));
}

void xml_dump_prolog(FILE* dest) {
  
  fprintf(dest, "<?xml version=\"1.0\" encoding=\"iso-8859-1\" standalone=\"yes\"?>\n");
  fprintf(dest, "<service_list>\n");
/*
  fprintf(dest, "<!DOCTYPE service_list SYSTEM \"http://wirbel.htpc-forum.de/w_scan/dtd/service_list.dtd\">\n");
  fprintf(dest, "\n");
  fprintf(dest, "<!-- NOTE:\n");
  fprintf(dest, "     if reading or writing w_scan XML file format:\n");
  fprintf(dest, "        - please validate XML against DTD above.\n");
  fprintf(dest, "        - indent each XML element\n");
  fprintf(dest, "        - indent using three spaces, dont use <TAB> char to indent.\n");
  fprintf(dest, "        - conform to requirements mentionend in DTD file.\n");
  fprintf(dest, " -->\n\n");
*/
}

void xml_dump_epilog(FILE* dest) {
  fprintf(dest, "</service_list>\n");
}

void xml_dump_service_parameter_set (FILE * f, struct service * s, struct transponder * t, struct w_scan_flags * flags)
{
  char buffer[1024] = { 0 };
  
	  int indent = 2;
	  
		//if (s->video_pid || s->audio_pid[0]) {
		//        if (s->provider_name)
		//                fprintf (f, "%s(%s):", s->service_name, s->provider_name);
		//        else
		//                fprintf (f, "%s:", s->service_name);
		//        xml_dump_dvb_parameters (f, t, flags);
		//        fprintf (f, ":%i:%i:%i", s->video_pid, s->ac3_pid[0]?s->ac3_pid[0]:s->audio_pid[0], s->service_id);
		//        /* what about AC3 audio here && multiple audio pids? see also: dump_mplayer.c/h */
		//        fprintf (f, "\n");
		//        }
	  
	  fprintf(f, "%s<service ONID=\"%u\" TSID=\"%u\" SID=\"%u\">\n",
         get_indent(indent), t->original_network_id, t->transport_stream_id, s->service_id);
	  indent++;
	  
	//   fprintf(f, "%s<network_id>%u</network_id>\n", get_indent(indent), t->original_network_id);
	//   fprintf(f, "%s<stream_id>%u</stream_id>\n", get_indent(indent), t->transport_stream_id);
	//   fprintf(f, "%s<service_id>%u</service_id>\n", get_indent(indent), s->service_id);

	  xml_encode_entities(s->service_name, buffer);
	  //fprintf(f, "%s<name char256=\"%s\"/>\n", get_indent(indent), s->service_name);
	  fprintf(f, "%s<name char256=\"%s\"/>\n", get_indent(indent), buffer);
	  fprintf(f, "%s<provider char256=\"%s\"/>\n", get_indent(indent), s->provider_name);
	  fprintf(f, "%s<pcr pid=\"%u\"/>\n", get_indent(indent), s->pcr_pid);
	  fprintf(f, "%s<logical channel_number=\"%u\"/>\n", get_indent(indent), s->logical_channel_number);

      fprintf(f, "%s<streams>\n", get_indent(indent));
      indent++;
      // Video Stream
      fprintf(f, "%s<stream type=\"%u\" pid=\"%u\" description=\"%s\"/>\n",
         get_indent(indent),
         s->video_stream_type,
         s->video_pid,
         get_stream_type_description(s->video_stream_type));
      // Audio Streams
      for (int i=0; i<AUDIO_CHAN_MAX; i++) {
         if (s->audio_stream_type[i])
            fprintf(f, "%s<stream type=\"%u\" pid=\"%u\" description=\"%s\" language_code=\"%s\"/>\n",
               get_indent(indent),
               s->audio_stream_type[i],
               s->audio_pid[i],
               get_stream_type_description(s->audio_stream_type[i]),
               s->audio_lang[i]);
      }
      // AC3 Streams
      for (int i=0; i<AC3_CHAN_MAX; i++) {
         if (s->ac3_stream_type[i])
            fprintf(f, "%s<stream type=\"%u\" pid=\"%u\" description=\"%s\" language_code=\"%s\"/>\n",
               get_indent(indent),
               s->ac3_stream_type[i],
               s->ac3_pid[i],
               get_stream_type_description(s->ac3_stream_type[i]),
               s->ac3_lang[i]);
      }
      indent--;
      fprintf(f, "%s</streams>\n", get_indent(indent));
      fprintf(f, "%s<subtitles>\n", get_indent(indent));
      indent++;
      // Subtitles
      for (int i=0; i<SUBTITLES_MAX; i++) {
         if (s->subtitling_pid[i])
            fprintf(f, "%s<subtitle pid=\"%u\" type=\"%u\" composition_page=\"%u\" ancillary_page=\"%u\" language_code=\"%s\"/>\n",
               get_indent(indent),
               s->subtitling_pid[i],
               s->subtitling_type[i],
               s->composition_page_id[i],
               s->ancillary_page_id[i],
               s->subtitling_lang[i]);
      }
      indent--;
      fprintf(f, "%s</subtitles>\n", get_indent(indent));
      fprintf(f, "%s<CA_systems>\n", get_indent(indent));
      indent++;
      for (int i=0; i<CA_SYSTEM_ID_MAX; i++) {
         if (s->ca_id[i])
            fprintf(f, "%sCA_system name=\"CA_System_Dummy\" ca_id=\"%u\"\n", get_indent(indent),s->ca_id[i]);
      }
      indent--;
      fprintf(f, "%s</CA_systems>\n", get_indent(indent));
      fprintf(f, "%s<comment char256=\"\"/>\n", get_indent(indent));
	  indent--;
	  fprintf(f, "%s</service>\n", get_indent(indent));
}


/*

<service_list>
   <services>
      <service ONID="8438" TSID="13057" SID="102">
         <name char256="Nelonen Pro 2 HD"/>
         <provider char256="DNA"/>
         <pcr pid="213"/>
         <streams>
            <stream type="27" pid="213" description="AVC Video stream, ITU-T Rec. H.264 | ISO/IEC 14496-10"/>
            <stream type="4" pid="330" description="AUDIO" language_code="fin"/>
         </streams>
         <CA_systems>
            <CA_system name="Conax" ca_id="0x0B00"/>
            <CA_system name="Conax" ca_id="0x0B02"/>
         </CA_systems>
         <comment char256="Nelonen Pro 2 HD is twice on 177.5MHz"/>
      </service>
   </services>
</service_list>
*/
void xml_dump_services_open(FILE* dest) { fprintf(dest, "%s<services>\n", get_indent(1)); }
void xml_dump_services_close(FILE* dest) { fprintf(dest, "%s</services>\n", get_indent(1)); }

void xml_encode_entities(char *src, char *dst)
{
  if (src != NULL) {
    
    int len = 0;
    int i   = 0;
    int a   = 0;
    
    len = strlen(src);
    
    for (i=0;i<len;i++) {
      
      switch ( src[i] ) {
        case '"':
          sprintf(&dst[a], "&quot;");
          a += 6;
          break;
        case '\'':
          sprintf(&dst[a], "&apos;");
          a += 6;
          break;
        case '<':
          sprintf(&dst[a], "&lt;");
          a += 4;
          break;
        case '>':
          sprintf(&dst[a], "&gt;");
          a += 4;
          break;
        case '&':
          sprintf(&dst[a], "&amp;");
          a += 5;
          break;
        default:
          dst[a] = src[i];
          a++;
          break;
      }
    }
  }
}
