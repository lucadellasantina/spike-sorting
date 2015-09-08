/* main.cc
 *
 * Entry point for the extract command-line tool
 *
 * (C) 2015 Benjamin Naecker bnaecker@stanford.edu
 */

#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>

#include <algorithm>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <vector>
#include <numeric>

#include <armadillo>

#include "snipfile.h"
#include "hidensfile.h"
#include "hidenssnipfile.h"
#include "extract.h"

#define UL_PRE "\033[4m"
#define UL_POST "\033[0m"
#define DEFAULT_THRESHOLD 4.5
#define DEFAULT_THRESH_STR "4.5"
#define VERSION_MAJOR 0
#define VERSION_MINOR 3
#define HIDENS_CHANNEL_MAX 127
#define HIDENS_CHANNEL_MIN 1
#define MCS_CHANNEL_MAX 64
#define MCS_CHANNEL_MIN 4

using sampleMat = arma::Mat<short>;

const char PROGRAM[] = "extract";
const char AUTHOR[] = "Benajmin Naecker";
const char AUTHOR_EMAIL[] = "bnaecker@stanford.edu";
const char YEAR[] = "2015";
const char USAGE[] = "\n\
 Usage: extract [-v | --version] [-h | --help]\n\
  \t\t[-t | --threshold " UL_PRE "threshold" UL_POST "]\n\
  \t\t[-b | --before " UL_PRE "nbefore" UL_POST "]\n\
  \t\t[-a | --after " UL_PRE "nafter" UL_POST "]\n\
  \t\t[-n | --nrandom " UL_PRE "nrandom" UL_POST "]\n\
  \t\t[-c | --chan " UL_PRE "chan-list" UL_POST "]\n\
  \t\t[-o | --output " UL_PRE"name" UL_POST "]\n\
  \t\t" UL_PRE "recording-file" UL_POST "\n\n\
 Extract noise and spike snippets from the given recording file\n\n\
 Parameters:\n\n\
   " UL_PRE "threshold" UL_POST "\tThreshold multiplier for determining spike\n\
   \t\tsnippets. The threshold will be set independently for each channel, such\n\
   \t\tthat: " UL_PRE "threshold" UL_POST " * median(abs(v)). Default = %0.1f\n\n\
   " UL_PRE "nbefore" UL_POST "\tNumber of samples before a spike peak to extract.\n\
   \t\tDefaults to %zu for MCS arrays and %zu for HiDens arrays.\n\n\
   " UL_PRE "nafter" UL_POST "\tNumber of samples after a spike peak to extract.\n\
   \t\tDefaults to %zu for MCS arrays and %zu for HiDens arrays.\n\n\
   " UL_PRE "nrandom" UL_POST "\tThe number of random snippets to extract.\n\n\
   " UL_PRE "chan" UL_POST "\t\tA comma- or dash-separated list of channels from which\n\
   \t\tsnippets will be extracted. E.g., \"0,1,2,3\" will extract data only from\n\
   \t\tthe first 4 channels, while \"0-4,8,10-\" will extract data from the first\n\
   \t\t4 channels, the 9th, and channels 11 to the number of channels in the file.\n\
   \t\tFor MCS data files, the default is \"3-63\", and for Hidens files, the default\n\
   \t\tis \"1-\". Note that ranges are half-open, so that the range specified as \"3-15\"\n\
   \t\twill collect channels 4 through 14, inclusive, but not channel 15. Also note\n\
   \t\tthat indexing is 0-based.\n\n\
   " UL_PRE "output" UL_POST "\tThe base-name for the output snippet file, which contains\n\
   \t\tboth random and noise snippets. It will be named as " UL_PRE "basename" UL_POST ".snip\n\n";

void print_usage_and_exit()
{
	printf(USAGE, DEFAULT_THRESHOLD, snipfile::NUM_SAMPLES_BEFORE, 
			hidenssnipfile::NUM_SAMPLES_BEFORE, snipfile::NUM_SAMPLES_AFTER,
			hidenssnipfile::NUM_SAMPLES_AFTER);
	exit(EXIT_SUCCESS);
}

void print_version_and_exit()
{
	std::cout << PROGRAM << " version " << VERSION_MAJOR << "." << VERSION_MINOR << std::endl;
	std::cout << "(C) " << YEAR << " " << AUTHOR << " " << AUTHOR_EMAIL << std::endl;
	exit(EXIT_SUCCESS);
}

size_t channel_min(std::string array)
{
	if (array == "unknown" || array == "hidens") 
		return HIDENS_CHANNEL_MIN;
	return MCS_CHANNEL_MIN;
}

size_t channel_max(std::string array)
{
	if (array == "unknown" || array == "hidens") 
		return HIDENS_CHANNEL_MAX;
	return MCS_CHANNEL_MAX;
}

bool is_hidens(std::string array)
{
	return (array == "hidens");
}

H5::DataType get_array_dtype(std::string array)
{
	return (array == "hidens") ? H5::PredType::STD_U8LE : H5::PredType::STD_I16LE;
}

void parse_chan_list(std::string arg, arma::uvec& channels, 
		unsigned int max)
{
	/* Split the input string on ','. Each element is either a single channel
	 * or a dash-separated list of channels
	 */
	std::vector<std::string> ss;
	std::string tmp;
	for (auto& c : arg) {
		if (c == ',') {
			ss.push_back(tmp);
			tmp.erase();
		} else {
			tmp.append(1, c);
		}
	}
	if (!tmp.empty())
		ss.push_back(tmp);

	/* Parse each element. Singleton strings are expected to be integers,
	 * and are added directly. Others are dash-separated; the two ends are
	 * converted to ints, and then everything between is filled in.
	 */
	try { 
		for (auto& each : ss) {
			auto dash = each.find('-');
			if (dash == std::string::npos) {
				auto x = std::stoul(each);
				channels.resize(channels.n_elem + 1);
				channels(channels.n_elem - 1) = x;
			} else {
				size_t pos;
				unsigned long start;
				start = std::stoul(each, &pos);
				unsigned long end;
				if (each.back() == '-')
					end = max;
				else {
					auto tmp = std::stoul(each.substr(dash + 1));
					end = (tmp > max) ? max : tmp;
				}
				auto num = end - start;
				channels.resize(channels.n_elem + num);
				for (decltype(num) i = 0; i < num; i++)
					channels(channels.n_elem - i - 1) = start + i;
			}
		}
	} catch ( std::exception& e ) {
		std::cerr << "Invalid channel list: " << arg << std::endl;
		exit(EXIT_FAILURE);
	}
	std::sort(channels.begin(), channels.end());
	std::unique(channels.begin(), channels.end());
}

void parse_command_line(int argc, char **argv, 
		double& thresh, size_t& nrandom_snippets, int& nbefore, int& nafter,
		std::string& chan_arg, std::string& output, std::string& filename)
{
	if (argc == 1)
		print_usage_and_exit();

	/* Parse options */
	struct option options[] = {
		{ "threshold", 	required_argument, 	nullptr, 't' },
		{ "before", 	required_argument, 	nullptr, 'b' },
		{ "after", 		required_argument, 	nullptr, 'a' },
		{ "help", 		no_argument, 		nullptr, 'h' },
		{ "version", 	no_argument, 		nullptr, 'v' },
		{ "chan", 		required_argument, 	nullptr, 'c' },
		{ "nrandom", 	required_argument,  nullptr, 'n' },
		{ "output", 	required_argument,  nullptr, 'o' },
		{ nullptr, 		0, 					nullptr, 0 	 }
	};
	int opt;
	while ( (opt = getopt_long(argc, argv, "hvo:t:c:n:", options, nullptr)) != -1 ) {
		switch (opt) {
			case 'h':
				print_usage_and_exit();
			case 'v':
				print_version_and_exit();
			case 't':
				try {
					thresh = std::stof(std::string(optarg));
				} catch ( ... ) {
					std::cerr << "Invalid threshold: " << optarg << std::endl;
					exit(EXIT_FAILURE);
				}
				break;
			case 'b':
				nbefore = std::stoi(std::string(optarg));
				break;
			case 'a':
				nafter = std::stoi(std::string(optarg));
				break;
			case 'c':
				chan_arg = std::string(optarg);
				break;
			case 'o':
				output = std::string(optarg);
				break;
			case 'n':
				nrandom_snippets = std::stoul(std::string(optarg));
				break;
		}
	}
	argv += optind;
	argc -= optind;
	if (argc == 0)
		print_usage_and_exit();
	filename = std::string(argv[0]);
	
	if (output.empty()) {
		size_t pos = filename.rfind(".");
		output = filename.substr(0, pos);
		std::string output_name = output + snipfile::FILE_EXTENSION;
		struct stat buf;
		if (stat(output_name.c_str(), &buf) == 0) {
			std::cerr << "Output file already exists: " + output_name << std::endl;
			exit(EXIT_FAILURE);
		}
	}
}

std::string get_array(std::string filename)
{
	return datafile::DataFile(filename).array();
}

bool sequential_channels(const arma::uvec& channels)
{
	if (channels.n_elem == 1)
		return true;
	return !arma::any(channels(arma::span(1, channels.n_elem - 1)) -
			channels(arma::span(0, channels.n_elem - 2)) > 1);
}

void verify_channels(arma::uvec& channels, const datafile::DataFile* file)
{
	arma::uvec file_channels(file->nchannels(), arma::fill::zeros);
	std::iota(file_channels.begin(), file_channels.end(), 0);
	arma::uvec valid_channels(file_channels.n_elem, arma::fill::zeros);
	size_t nelem = 0;
	for (auto i = decltype(channels.n_elem){0}; i < channels.n_elem; i++) {
		if (arma::any(file_channels == channels(i))) {
			valid_channels(nelem) = channels(i);
			nelem++;
		}
	}
	valid_channels.resize(nelem);
	channels = valid_channels;
}

void verify_snippet_offsets(const std::string& array, int& nbefore, int& nafter)
{
	if (nbefore <= 0) {
		nbefore = ((array == "hidens") ? hidenssnipfile::NUM_SAMPLES_BEFORE :
				snipfile::NUM_SAMPLES_BEFORE);
	}
	if (nafter <= 0) {
		nafter = ((array == "hidens") ? hidenssnipfile::NUM_SAMPLES_AFTER :
				snipfile::NUM_SAMPLES_AFTER);
	}
}

int main(int argc, char *argv[])
{	
	/* Parse input and get the array type */
	auto thresh = DEFAULT_THRESHOLD;
	auto nrandom_snippets = snipfile::NUM_RANDOM_SNIPPETS;
	std::string chan_arg, output, filename;
	int nbefore = -1, nafter = -1;
	parse_command_line(argc, argv, thresh, nrandom_snippets, nbefore, nafter,
			chan_arg, output, filename);
	std::string array = get_array(filename);

	/* Verify the number of samples before/after a spike peak, based on array. */
	verify_snippet_offsets(array, nbefore, nafter);

	/* Get channels based on input */
	arma::uvec channels;
	if (chan_arg.empty()) {
		auto min = channel_min(array), max = channel_max(array);
		channels.set_size(max - min);
		std::iota(channels.begin(), channels.end(), min);
	} else
		parse_chan_list(chan_arg, channels, channel_max(array));

	/* Open the file and verify the channels requested */
	datafile::DataFile *file;
	if (array == "hidens")
		file = dynamic_cast<datafile::DataFile*>(new hidensfile::HidensFile(filename));
	else
		file = new datafile::DataFile(filename);
	if (!file) {
		std::cerr << "Could not cast Hidens file to base datafile" << std::endl;
		exit(EXIT_FAILURE);
	}
	verify_channels(channels, file);

	/* Read all data from the requested channels */
#ifdef DEBUG
	std::cout << "Loading data from channels: " << std::endl << channels;
#endif
	sampleMat data;
	if (sequential_channels(channels))
		file->data(channels.min(), channels.max() + 1, 
				0, file->nsamples(), data);
	else
		file->data(channels, 0, file->nsamples(), data);

	/* Create snippet file */
	snipfile::SnipFile *snip_file;
	if (array == "hidens")
		snip_file = dynamic_cast<hidenssnipfile::HidensSnipFile*>(
				new hidenssnipfile::HidensSnipFile(output + 
				snipfile::FILE_EXTENSION, *dynamic_cast<hidensfile::HidensFile*>(file),
				nbefore, nafter));
	else
		snip_file = new snipfile::SnipFile(
				output + snipfile::FILE_EXTENSION, *file, nbefore, nafter);

	/* Compute thresholds */
#ifdef DEBUG
	std::cout << "Computing thresholds" << std::endl;
#endif
	extract::meanSubtract(data);
	auto thresholds = extract::computeThresholds(data, thresh);

	/* Find noise and spike snippets */
	auto nchannels = channels.size();
	std::vector<arma::uvec> spike_idx(nchannels), noise_idx(nchannels);
	std::vector<sampleMat> spike_snips(nchannels), noise_snips(nchannels);
	extract::extractNoise(data, nrandom_snippets, nbefore, nafter,
			noise_idx, noise_snips);
	extract::extractSpikes(data, thresholds, nbefore, nafter, 
			spike_idx, spike_snips);

	/* Write snippets to disk */
	snip_file->setChannels(channels);
	snip_file->setThresholds(thresholds);
	snip_file->writeSpikeSnips(spike_idx, spike_snips);
	snip_file->writeNoiseSnips(noise_idx, noise_snips);

	/* Cleanup */
	delete file;
	delete snip_file;
	
	return 0;
}

