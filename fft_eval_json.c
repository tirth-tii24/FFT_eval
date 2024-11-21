/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2012 Simon Wunderlich <sw@simonwunderlich.de>
 * SPDX-FileCopyrightText: 2012 Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V.
 * SPDX-FileCopyrightText: 2013 Gui Iribarren <gui@altermundi.net>
 * SPDX-FileCopyrightText: 2017 Nico Pace <nicopace@altermundi.net>
 */

/*
 * This program has been created to aid open source spectrum
 * analyzer development for Qualcomm/Atheros AR92xx and AR93xx
 * based chipsets.
 */

#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fft_eval.h"

#define MAX_RSSI_SUPPORT 100
int rssi_list[MAX_RSSI_SUPPORT] = {0};
int req_freq = 0;

int get_data(int rssi)
{
	int i,handle=0,data,bins;
	double sum = 0.0, sum_sq = 0.0, mean, variance, index = 0.0;
	struct scanresult *result;
	memset(rssi_list,0,sizeof(int)*100);
	for (result = result_list; result; result = result->next) {
		switch (result->sample.tlv.type) {

		case ATH_FFT_SAMPLE_HT20:
			if (result->sample.ht20.rssi != rssi)
				continue;
			for (i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
				data = result->sample.ht20.data[i] << result->sample.ht20.max_exp;
				if (data == 0)
					data = 1;
			}
			handle=1;
			break;
		case ATH_FFT_SAMPLE_HT20_40:
			if (result->sample.ht40.lower_rssi != rssi)
				continue;
			for (i = 0; i < SPECTRAL_HT20_40_NUM_BINS; i++) {
				data = result->sample.ht40.data[i] << result->sample.ht40.max_exp;
				if (data == 0)
					data = 1;
			}
			handle=1;
			break;
		case ATH_FFT_SAMPLE_ATH10K:
			if (result->sample.ath10k.header.rssi != rssi)
				continue;
			bins = result->sample.tlv.length - (sizeof(result->sample.ath10k.header) - sizeof(result->sample.ath10k.header.tlv));
			for (i = 0; i < bins; i++) {
				data = result->sample.ath10k.data[i] << result->sample.ath10k.header.max_exp;
				if (data == 0)
					data = 1;
				sum += data;        // Sum of all elements
				sum_sq += data * data;  // Sum of squares of elements
			}
			handle=1;
			break;
		case ATH_FFT_SAMPLE_ATH11K:
			if (result->sample.ath11k.header.rssi != rssi)
				continue;
			bins = result->sample.tlv.length - (sizeof(result->sample.ath11k.header) - sizeof(result->sample.ath11k.header.tlv));
			for (i = 0; i < bins; i++) {
				data = result->sample.ath11k.data[i] << result->sample.ath11k.header.max_exp;
				if (data == 0)
					data = 1;
			}
			handle=1;
			break;
	}
	if (handle)
		break;
	}
        mean = sum / bins;
	variance = (sum_sq / bins) - (mean * mean);
        index = (0.5 * rssi) + (0.3 * mean) + (0.2 * variance);
	if ( index > 100 )
		index = 100;
        printf("[{\"rssi\":%d},{\"data_mean\":%.2f},{\"data_vari\":%2.f},{\"index\":%.2f}]\n", rssi, mean, variance,index);
	return handle;
}

static int index_rssi(void)
{
	int i,max_samples=0, rssi=0;
	struct scanresult *result;

	memset(rssi_list,0,sizeof(int)*100);
	for (result = result_list; result; result = result->next) {
		switch (result->sample.tlv.type) {

		case ATH_FFT_SAMPLE_HT20:
			if ((req_freq != 0) && ( req_freq !=  result->sample.ht20.freq))
				continue;
			if (result->sample.ht20.rssi < MAX_RSSI_SUPPORT)
				rssi_list[result->sample.ht20.rssi]++;
			break;
		case ATH_FFT_SAMPLE_HT20_40:
			if ((req_freq != 0) && ( req_freq !=  result->sample.ht40.freq))
				continue;
			if (result->sample.ht40.lower_rssi < MAX_RSSI_SUPPORT)
				rssi_list[result->sample.ht40.lower_rssi]++;
			break;
		case ATH_FFT_SAMPLE_ATH10K:
			if ((req_freq != 0) && ( req_freq !=  result->sample.ath10k.header.freq1))
				continue;
			if (result->sample.ath10k.header.rssi < MAX_RSSI_SUPPORT)
				rssi_list[result->sample.ath10k.header.rssi]++;
			break;
		case ATH_FFT_SAMPLE_ATH11K:
			if ((req_freq != 0) && ( req_freq !=  result->sample.ath11k.header.freq1))
				continue;
			if (result->sample.ath11k.header.rssi < MAX_RSSI_SUPPORT)
				rssi_list[result->sample.ath11k.header.rssi]++;
			break;
	}
	}
	for(i=0;i<MAX_RSSI_SUPPORT;i++)
	{
		if (rssi_list[i] > max_samples) {
			max_samples = rssi_list[i];
			rssi = i;
		}
	}
	//printf("Rssi %d has %d samples\n",rssi,max_samples);
	return rssi;
}



/*
 * print_values - spit out the analyzed values in text form, JSON-like.
 */
static int print_values(void)
{
	int i, rnum;
	struct scanresult *result;

	printf("[");
	rnum = 0;
	if (!result_list)
		printf("No data\n");
	for (result = result_list; result; result = result->next) {

		switch (result->sample.tlv.type) {

		case ATH_FFT_SAMPLE_HT20:
			{
				int datamax = 0, datamin = 65536;
				int datasquaresum = 0;

				/* prints some statistical data about the
				 * data sample and auxiliary data. */
				printf("\n{ \"tsf\": %" PRIu64 ", \"central_freq\": %d, \"rssi\": %d, \"noise\": %d, \"data\": [ ", result->sample.ht20.tsf, result->sample.ht20.freq, result->sample.ht20.rssi,
				       result->sample.ht20.noise);
				for (i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
					int data;
					data = (result->sample.ht20.data[i] << result->sample.ht20.max_exp);
					data *= data;
					datasquaresum += data;
					if (data > datamax)
						datamax = data;
					if (data < datamin)
						datamin = data;
				}
				for (i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
					float freq;
					float signal;
					int data;
					freq = result->sample.ht20.freq - 10.0 + ((20.0 * i) / SPECTRAL_HT20_NUM_BINS);

					/* This is where the "magic" happens: interpret the signal
					 * to output some kind of data which looks useful.  */

					data = result->sample.ht20.data[i] << result->sample.ht20.max_exp;
					if (data == 0)
						data = 1;
					signal = result->sample.ht20.noise + result->sample.ht20.rssi + 20 * log10(data) - log10(datasquaresum) * 10;

					printf("[ %f, %f ]", freq, signal);
					if (i < SPECTRAL_HT20_NUM_BINS - 1)
						printf(", ");
				}
			}
			break;
		case ATH_FFT_SAMPLE_HT20_40:
			{
				int datamax = 0, datamin = 65536;
				int datasquaresum_lower = 0;
				int datasquaresum_upper = 0;
				int datasquaresum;
				int i;
				int centerfreq;
				s8 noise;
				s8 rssi;
				//todo build average

				printf("\n{ \"tsf\": %" PRIu64 ", \"central_freq\": %d, \"rssi\": %d, \"noise\": %d, \"data\": [ ", result->sample.ht40.tsf, result->sample.ht40.freq, result->sample.ht40.lower_rssi,
				       result->sample.ht40.lower_noise);
				for (i = 0; i < SPECTRAL_HT20_40_NUM_BINS / 2; i++) {
					int data;

					data = result->sample.ht40.data[i];
					data <<= result->sample.ht40.max_exp;
					data *= data;
					datasquaresum_lower += data;

					if (data > datamax)
						datamax = data;
					if (data < datamin)
						datamin = data;
				}

				for (i = SPECTRAL_HT20_40_NUM_BINS / 2; i < SPECTRAL_HT20_40_NUM_BINS; i++) {
					int data;

					data = result->sample.ht40.data[i];
					data <<= result->sample.ht40.max_exp;
					datasquaresum_upper += data;

					if (data > datamax)
						datamax = data;
					if (data < datamin)
						datamin = data;
				}

				switch (result->sample.ht40.channel_type) {
				case NL80211_CHAN_HT40PLUS:
					centerfreq = result->sample.ht40.freq + 10;
					break;
				case NL80211_CHAN_HT40MINUS:
					centerfreq = result->sample.ht40.freq - 10;
					break;
				default:
					return -1;
				}

				for (i = 0; i < SPECTRAL_HT20_40_NUM_BINS; i++) {
					float freq;
					int data;

					freq = centerfreq - (40.0 * SPECTRAL_HT20_40_NUM_BINS / 128.0) / 2 + (40.0 * (i + 0.5) / 128.0);

					if (i < SPECTRAL_HT20_40_NUM_BINS / 2) {
						noise = result->sample.ht40.lower_noise;
						datasquaresum = datasquaresum_lower;
						rssi = result->sample.ht40.lower_rssi;
					} else {
						noise = result->sample.ht40.upper_noise;
						datasquaresum = datasquaresum_upper;
						rssi = result->sample.ht40.upper_rssi;
					}

					data = result->sample.ht40.data[i];
					data <<= result->sample.ht40.max_exp;
					if (data == 0)
						data = 1;

					float signal = noise + rssi + 20 * log10(data) - log10(datasquaresum) * 10;

					printf("[ %f, %f ]", freq, signal);
					if (i < SPECTRAL_HT20_40_NUM_BINS - 1)
						printf(", ");
				}
			}
			break;
		case ATH_FFT_SAMPLE_ATH10K:
			{
				int datamax = 0, datamin = 65536;
				int datasquaresum = 0;
				int i, bins;
				printf("\n{ \"tsf\": %" PRIu64 ", \"central_freq\": %d, \"rssi\": %d, \"noise\": %d, \"data\": [ ", result->sample.ath10k.header.tsf, result->sample.ath10k.header.freq1,
				       result->sample.ath10k.header.rssi, result->sample.ath10k.header.noise);

				bins = result->sample.tlv.length - (sizeof(result->sample.ath10k.header) - sizeof(result->sample.ath10k.header.tlv));

				for (i = 0; i < bins; i++) {
					int data;

					data = (result->sample.ath10k.data[i] << result->sample.ath10k.header.max_exp);
					data *= data;
					datasquaresum += data;
					if (data > datamax)
						datamax = data;
					if (data < datamin)
						datamin = data;
				}

				for (i = 0; i < bins; i++) {
					float freq;
					int data;
					float signal;
					freq = result->sample.ath10k.header.freq1 - (result->sample.ath10k.header.chan_width_mhz) / 2 + (result->sample.ath10k.header.chan_width_mhz * (i + 0.5) / bins);

					data = result->sample.ath10k.data[i] << result->sample.ath10k.header.max_exp;
					if (data == 0)
						data = 1;
					signal = result->sample.ath10k.header.noise + result->sample.ath10k.header.rssi + 20 * log10(data) - log10(datasquaresum) * 10;
					printf("[ %f, %f ]", freq, signal);
					if (i < bins - 1)
						printf(", ");

				}

			}
			break;
		case ATH_FFT_SAMPLE_ATH11K:
			{
				int datamax = 0, datamin = 65536;
				int datasquaresum = 0;
				int i, bins;
				printf("\n{ \"tsf\": %08d, \"central_freq\": %d, \"rssi\": %d, \"noise\": %d, \"data\": [ ", result->sample.ath11k.header.tsf, result->sample.ath11k.header.freq1,
				       result->sample.ath11k.header.rssi, result->sample.ath11k.header.noise);

				bins = result->sample.tlv.length - (sizeof(result->sample.ath11k.header) - sizeof(result->sample.ath11k.header.tlv));

				for (i = 0; i < bins; i++) {
					int data;

					data = result->sample.ath11k.data[i];
					data *= data;
					datasquaresum += data;
					if (data > datamax)
						datamax = data;
					if (data < datamin)
						datamin = data;
				}

				for (i = 0; i < bins; i++) {
					float freq;
					int data;
					float signal;
					freq = result->sample.ath11k.header.freq1 - (result->sample.ath11k.header.chan_width_mhz) / 2 + (result->sample.ath11k.header.chan_width_mhz * (i + 0.5) / bins);

					data = result->sample.ath11k.data[i];
					if (data == 0)
						data = 1;
					signal = result->sample.ath11k.header.noise + result->sample.ath11k.header.rssi + 20 * log10f(data) - log10f(datasquaresum) * 10;
					printf("[ %f, %f ]", freq, signal);
					if (i < bins - 1)
						printf(", ");

				}
			}
			break;
		}

		printf(" ] }");
		if (result->next)
			printf(",");
		rnum++;
	}
	printf("\n]\n");

	return 0;
}

static void usage(const char *prog)
{
	if (!prog)
		prog = "fft_eval";

	fprintf(stderr, "Usage: %s scanfile\n", prog);
	fft_eval_usage(prog);
}

int main(int argc, char *argv[])
{
	char *ss_name = NULL;
	char *prog = NULL;

	if (argc >= 1)
		prog = argv[0];

	if (argc >= 2)
		ss_name = argv[1];

	if (argc >= 3)
		req_freq = atoi(argv[2]);

	//fprintf(stderr, "WARNING: Experimental Software! Don't trust anything you see. :)\n");
	//fprintf(stderr, "\n");

	if (fft_eval_init(ss_name) < 0) {
		fprintf(stderr, "Couldn't read scanfile ...\n");
		usage(prog);
		return -1;
	}
	int rssi = index_rssi();
	get_data(rssi);
	//print_values();
	fft_eval_exit();

	return 0;
}
