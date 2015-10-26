/* extract.cc
 *
 * Library components to extract noise and spike snippets from raw data.
 *
 * (C) 2015 Benjamin Naecker bnaecker@stanford.edu
 */

#include "extract.h"
#include "snipfile.h"

void extract::randsample(std::vector<arma::uvec>& out, size_t min, size_t max)
{
	size_t min_size = arma::datum::inf;
	for (auto& each : out) {
		if (each.n_elem < min_size)
			min_size = each.n_elem;
	}
	if (min_size > (max - min))
		throw std::logic_error(
				"Number of requested elems must be less than (max - min)");

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dist(min, max - 1);
	for (decltype(out.size()) c = 0; c < out.size(); c++) {
		auto& v = out.at(c);
		for (arma::uword i = 0; i < v.n_elem; i++)
			v(i) = dist(gen);
	}
}

arma::vec extract::meanSubtract(sampleMat& data)
{
	arma::vec means(data.n_cols, arma::fill::zeros);
	for (auto i = decltype(data.n_cols){0}; i < data.n_cols; i++) {
		means(i) = arma::mean(arma::conv_to<arma::vec>::from(data.col(i)));
		data.col(i) -= means(i);
	}
	return means;
}

arma::vec extract::computeThresholds(const sampleMat& data, double thresh)
{
	return thresh * arma::conv_to<arma::vec>::from(arma::median(arma::abs(data)));
}

bool extract::isLocalMax(const sampleMat& data, size_t channel, 
		size_t sample, size_t n)
{
	/* Compute box-car average of samples in data(i, j) of size n,
	 * and return true if mid-point is a local maximum.
	 */
	arma::vec tmp(n);
	auto mid = std::floor(n / 2);
	for (decltype(n) k = 0; k < n; k++)
		tmp(k) = arma::accu(data(
				arma::span(sample - n + k + 1, sample + k), channel)) / n;
	return (arma::all(tmp(arma::span(0, mid - 1)) < tmp(mid)) && 
			arma::all(tmp(arma::span(mid, n - 1)) <= tmp(mid)));
}

void extract::extractNoise(const sampleMat& data, const size_t& nrandom_snippets,
		const int& nbefore, const int& nafter, std::vector<arma::uvec>& idx, 
		std::vector<sampleMat>& snips)
{
	/* Create random indices into each channel */
	auto nsamples_per_snip = nbefore + nafter + 1;
	auto nsamples = data.n_rows, nchannels = data.n_cols;
	for (auto& each : idx)
		each.set_size(nrandom_snippets);
	randsample(idx, nbefore, nsamples - nafter);

#ifdef DEBUG
	std::cout << "Extracting noise snippets" << std::endl;
#endif

	/* Extract snippets at those random indices */
	for (decltype(nchannels) c = 0; c < nchannels; c++) {

#ifdef DEBUG
		std::cout << " Channel " << c << std::endl;
#endif

		auto& snip_mat = snips.at(c);
		snip_mat.set_size(nsamples_per_snip, nrandom_snippets);
		auto& ix = idx.at(c);
		for (auto s = decltype(nrandom_snippets){0}; s < nrandom_snippets; s++) {
			auto& start = ix.at(s);
			snip_mat(arma::span::all, s) = data(
					arma::span(start - nbefore, start + nafter), c);
		}
	}
}

void extract::extractSpikes(const sampleMat& data, const arma::vec& thresholds, 
		const int& nbefore, const int& nafter,
		std::vector<arma::uvec>& idx, std::vector<sampleMat>& snips)
{
	auto nsamples_per_snip = nbefore + nafter + 1;
	auto nsamples = data.n_rows, nchannels = data.n_cols;

#ifdef DEBUG
	std::cout << "Extracting spike snippets" << std::endl;
#endif 

	for (decltype(nchannels) c = 0; c < nchannels; c++) {
		auto& idx_vec = idx.at(c);
		auto& snip_mat = snips.at(c);
		auto& thresh = thresholds(c);
		snip_mat.set_size(nsamples_per_snip, snipfile::DEFAULT_NUM_SNIPPETS);
		idx_vec.set_size(snipfile::DEFAULT_NUM_SNIPPETS);
		size_t snip_num = 0;

		/* Find snippets */
		arma::uword i = nbefore;
		while (i < nsamples - nafter + 1) {
			if (data(i, c) > thresh) {
				if (isLocalMax(data, c, i, snipfile::WINDOW_SIZE)) {
					if (snip_num >= snip_mat.n_cols) {
						snip_mat.resize(snip_mat.n_rows, 2 * snip_mat.n_cols);
						idx_vec.resize(2 * snip_mat.n_cols);
					}
					idx_vec(snip_num) = i;
					snip_mat(arma::span::all, snip_num) = data(
							arma::span(i - nbefore, i + nafter), c);
					snip_num++;
				}
			}
			/* XXX: Previous versions set this jump size as the smoothing window
			 * size if current sample was considered a local maximum, and one
			 * otherwise. Keeping it at 1 increases the number of snippets extracted
			 * by about 1%, but these extra snippets are probably doubly-counted
			 * versions of the same snippet.
			 */
			i += 1;
		}
		snip_mat.resize(snip_mat.n_rows, snip_num);
		idx_vec.resize(snip_num);

#ifdef DEBUG
		std::cout << " Channel " << c << ": " << snip_num << " snippets" << std::endl;
#endif

	}
}

