/*! \file datafile-templates.h
 * Includes template function definitions used in the DataFile class.
 */

template<class T>
void datafile::DataFile::_read_data(
		const size_t startChan, const size_t endChan, 
		const size_t start, const size_t end, T& out)
{
	/* Verify input and resize return array */
	if (end <= start) {
		std::cerr << "Requested sample range is invalid: Samples" 
			<< start << " - " << end << std::endl;
		throw std::logic_error("Requested sample range invalid");
	}
	size_t nreqSamples = end - start;
	if (endChan <= startChan) {
		std::cerr << "Requested sample range is invalid: Channels " 
			<< startChan << " - " << endChan << std::endl;
		throw std::logic_error("Requested sample range invalid");
	}
	size_t nreqChannels = endChan - startChan;
	out.set_size(nreqSamples, nreqChannels);

	/* Select hyperslab from the file */
	hsize_t spaceOffset[datafile::DATASET_RANK] = {startChan, start};
	hsize_t spaceCount[datafile::DATASET_RANK] = {nreqChannels, nreqSamples};
	dataspace.selectHyperslab(H5S_SELECT_SET, spaceCount, spaceOffset);
	if (!dataspace.selectValid()) {
		std::cerr << "Dataset selection invalid" << std::endl;
		std::cerr << "Offset: (, "<< startChan << ", " << start << ")" << std::endl;
		std::cerr << "Count: (" << nreqChannels << ", " << nreqSamples << ")" << std::endl;
		throw std::logic_error("Dataset selection invalid");
	}

	/* Define memory dataspace */
	hsize_t mdims[datafile::DATASET_RANK] = {nreqChannels, nreqSamples};
	H5::DataSpace mspace(datafile::DATASET_RANK, mdims);
	hsize_t moffset[datafile::DATASET_RANK] = {0, 0};
	hsize_t mcount[datafile::DATASET_RANK] = {nreqChannels, nreqSamples};
	mspace.selectHyperslab(H5S_SELECT_SET, mcount, moffset);
	if (!mspace.selectValid()) {
		std::cerr << "Memory dataspace selection invalid" << std::endl;
		std::cerr << "Count: (" << nreqSamples << ", " << nreqChannels << ")" << std::endl;
		throw std::logic_error("Memory dataspace selection invalid");
	}

	/* Get datatype of memory data space and read */
	H5::DataType dtype;
	auto hash = typeid(T).hash_code();
	if (hash == typeid(arma::mat).hash_code())
		dtype = H5::PredType::IEEE_F64LE;
	else if (hash == typeid(arma::Mat<short>).hash_code())
		dtype = H5::PredType::STD_I16LE;
	else if (hash == typeid(arma::Mat<uint8_t>).hash_code())
		dtype = H5::PredType::STD_U8LE;
	dataset.read(out.memptr(), dtype, mspace, dataspace);

	/* Sign-invert the Hidens data */
	if (array() == "hidens")
		out *= -1;
}

template<class T>
void datafile::DataFile::_read_data(
		const arma::uvec& channels,
		const size_t start, const size_t end, T& out)
{
	/* Verify input and resize return array */
	if (end <= start) {
		std::cerr << "Requested sample range is invalid: Samples" 
			<< start << " - " << end << std::endl;
		throw std::logic_error("Requested sample range invalid");
	}
	size_t nreqSamples = end - start;
	size_t nreqChannels = channels.n_elem;
	arma::Mat<hsize_t> coords;
	hsize_t nelem;
	computeCoords(channels, start, end, &coords, &nelem);
	out.set_size(nreqSamples, nreqChannels);

	/* Select hyperslab from the file */
	dataspace.selectElements(H5S_SELECT_SET, nelem, coords.memptr());
	if (!dataspace.selectValid()) {
		std::cerr << "Dataset selection invalid" << std::endl;
		std::cerr << "Offset: (0, " << start << ")" << std::endl;
		std::cerr << "Count: (" << nchannels() << ", " << nreqSamples << ")" << std::endl;
		throw std::logic_error("Dataset selection invalid");
	}

	/* Define memory dataspace */
	hsize_t mdims[datafile::DATASET_RANK] = {nreqChannels, nreqSamples};
	H5::DataSpace mspace(datafile::DATASET_RANK, mdims);
	hsize_t moffset[datafile::DATASET_RANK] = {0, 0};
	hsize_t mcount[datafile::DATASET_RANK] = {nreqChannels, nreqSamples};
	mspace.selectHyperslab(H5S_SELECT_SET, mcount, moffset);
	if (!mspace.selectValid()) {
		std::cerr << "Memory dataspace selection invalid" << std::endl;
		std::cerr << "Count: (" << nreqSamples << ", " << nreqChannels << ")" << std::endl;
		throw std::logic_error("Memory dataspace selection invalid");
	}

	/* Get datatype of memory data space and read */
	H5::DataType dtype;
	auto hash = typeid(T).hash_code();
	if (hash == typeid(arma::mat).hash_code())
		dtype = H5::PredType::IEEE_F64LE;
	else if (hash == typeid(arma::Mat<short>).hash_code())
		dtype = H5::PredType::STD_I16LE;
	else if (hash == typeid(arma::Mat<uint8_t>).hash_code())
		dtype = H5::PredType::STD_U8LE;
	dataset.read(out.memptr(), dtype, mspace, dataspace);

	/* Sign-invert the Hidens data */
	if (array() == "hidens")
		out *= -1;
}
