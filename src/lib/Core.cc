/*************************************************************************
	> File Name: Core.cc
	> Author: Leosocy
	> Mail: 513887568@qq.com 
	> Created Time: 2017/07/30 00:11:24
 ************************************************************************/
#include <EDCC/Core.h>

using namespace EDCC;

//-----------------------------------Palmprint-------------------------------------

Palmprint::Palmprint()
{
	this->identity = "";
	this->imagePath = "";
}

Palmprint::Palmprint( const string &identity, const string &imagePath )
{
	this->identity = identity;
	this->imagePath = imagePath;
}

Palmprint::~Palmprint()
{
}

Palmprint& Palmprint::operator =( const Palmprint &src )
{
	this->identity = src.identity;
	this->imagePath = src.imagePath;
	this->image = src.image.clone();
	return *this;
}

void Palmprint::setPalmprintInfo( const string &identity, const string &imagePath)
{
	this->identity = identity;
	this->imagePath = imagePath;
}

cv::Mat& Palmprint::genOriImg()
{
	assert( this->imagePath != "" );
	image = imread( this->imagePath, CV_LOAD_IMAGE_COLOR );
	return image;
}

cv::Mat& Palmprint::genSpecImg( const cv::Size &imgSize, bool isGray )
{
	assert( this->imagePath != "" );
	image = imread( this->imagePath, CV_LOAD_IMAGE_COLOR );
	resize( image, image, imgSize );
	if( isGray ) {
		cvtColor( image, image, CV_BGR2GRAY );
	}
	return image;
}

//---------------------------------PalmprintCode--------------------------------

PalmprintCode::PalmprintCode( const Palmprint &oneInstance )
{
	this->instance = oneInstance;
}

PalmprintCode::~PalmprintCode()
{
}

void PalmprintCode::encodePalmprint( const cv::Size &imgSize, const cv::Size &gabKerSize, int numOfDirections, const cv::Size &lapKerSize ) 
{
	assert( lapKerSize.width % 2 == 1 && lapKerSize.width == lapKerSize.height );
	GaborFilter filter( gabKerSize, numOfDirections, GaborFilter::GABOR_KERNEL_REAL );
	Mat palmprintImage = this->instance.genSpecImg( imgSize );
	Mat gaborResult;
	enhanceImage( palmprintImage, palmprintImage, lapKerSize );
	filter.doGaborFilter( palmprintImage, gaborResult );
	vector< cv::Mat > resultVec;
	vector< cv::Mat >::iterator resultIte;
	split( gaborResult, resultVec );
	int height = palmprintImage.rows;
	int width = palmprintImage.cols;
	coding.C = Mat( palmprintImage.size(), CV_8S );
	coding.Cs = Mat( palmprintImage.size(), CV_8S );

	for( int h = 0; h < height; ++h ) {
		for( int w = 0; w < width; ++w ) {
			double maxResponse = -DBL_MAX;
			int maxDirection = -1;
			int Cleft = -1, Cright = -1;
			for( int d = 0; d < numOfDirections; ++d ) {
				double response = resultVec[d].at<double>(h, w); 
				if( response > maxResponse ) {
					maxResponse = response;
					maxDirection = d;
				}
			}
			coding.C.at<char>( h, w ) = maxDirection;
			if( maxDirection == numOfDirections - 1 ) {
				Cleft = 0;
			} else {
				Cleft = maxDirection + 1;
			}
			if( maxDirection == 0 ) {
				Cright = numOfDirections - 1;
			} else {
				Cright = maxDirection - 1;
			}
			coding.Cs.at<char>( h, w ) = resultVec[Cleft].at<double>( h, w ) >= 
				resultVec[Cright].at<double>( h, w ) ? 1 : 0;
		}
	}
}

void PalmprintCode::enhanceImage( const cv::Mat &src, cv::Mat &dst, const cv::Size &lapKerSize )
{
	Mat gaussian;
	GaussianBlur( src, gaussian, Size( 5, 5 ), 0, 0, BORDER_DEFAULT );
	Laplacian( gaussian, dst, CV_64F, lapKerSize.width );
	normalize( dst, dst, 0, 1, CV_MINMAX );
}

//---------------------------------EDCCoding------------------------------------

EDCCoding& EDCCoding::operator=( const EDCCoding &coding )
{
	this->C = coding.C.clone();
	this->Cs = coding.Cs.clone();
	return *this;
}

//---------------------------------GaborFilter-----------------------------------

GaborFilter::GaborFilter( const cv::Size &kernelSize, int numOfDirections, int kernelType )
{
	assert( kernelSize.width %2 == 1 && kernelSize.height % 2 == 1 );
	assert( kernelType == GaborFilter::GABOR_KERNEL_REAL || kernelType == GaborFilter::GABOR_KERNEL_IMAG || kernelType == GaborFilter::GABOR_KERNEL_MAG );
	assert( numOfDirections > 0 );
	this->kernelSize = kernelSize;
	this->numOfDirections = numOfDirections;
	this->kernelType = kernelType;
}

GaborFilter::~GaborFilter()
{
}

void GaborFilter::doGaborFilter( const cv::Mat &src, cv::Mat &dstMerge )
{
	vector< cv::Mat > dstVec;
	Mat dst;
	int gaborH = this->kernelSize.height;
	int gaborW = this->kernelSize.width;
	Mat gaborKernel;
	for( int direction = 0; direction < this->numOfDirections; ++direction ) {
		getGaborKernel( gaborKernel, gaborW, gaborH, 0, direction, this->kernelType );
		filter2D( src, dst, CV_64F, gaborKernel );
		normalize( dst, dst, 0, 1, CV_MINMAX );
		dstVec.push_back( dst.clone() );
	}
	merge( dstVec, dstMerge );
}

void GaborFilter::getGaborKernel( cv::Mat &gaborKernel, int kernelWidth, int kernelHeight,
		int dimension, int direction, int kernelType, double Kmax, double f,
		double sigma, int ktype )
{
	assert( ktype == CV_32F || ktype == CV_64F );  
	int half_width = kernelWidth / 2;
	int half_height = kernelHeight / 2;
	double Qu = CV_PI * direction / this->numOfDirections;
	double sqsigma = sigma * sigma;
	double Kv = Kmax / pow( f, dimension );
	double postmean = exp( -sqsigma / 2 );
	Mat kernel( kernelWidth, kernelHeight, ktype );
	Mat kernel_2( kernelWidth, kernelHeight, ktype );
	Mat kernel_mag( kernelWidth, kernelHeight, ktype );
	double tmp1, tmp2, tmp3;
	for ( int j = -half_height; j <= half_height; j++ ){
		for ( int i = -half_width; i <= half_width; i++ ){
			tmp1 = exp( -( Kv * Kv * ( j * j + i * i ) ) / ( 2 * sqsigma ) );
			tmp2 = cos( Kv * cos( Qu ) * i + Kv * sin( Qu ) * j ) - postmean;
			tmp3 = sin( Kv * cos( Qu ) * i + Kv * sin( Qu ) * j );

			if( kernelType == GaborFilter::GABOR_KERNEL_REAL ) {
				if (ktype == CV_32F) {
					kernel.at<float>( j + half_height, i + half_width ) =
					(float)( Kv * Kv * tmp1 * tmp2 / sqsigma );
				} else {
					kernel.at<double>( j + half_height, i + half_width ) =
					(double)( Kv * Kv * tmp1 * tmp2 / sqsigma );
				}
			} else if( kernelType == GaborFilter::GABOR_KERNEL_IMAG ) {
				if (ktype == CV_32F) {
					kernel.at<float>( j + half_height, i + half_width ) =
					(float)( Kv * Kv * tmp1 * tmp3 / sqsigma );
				} else {
					kernel.at<double>( j + half_height, i + half_width ) =
					(double)( Kv * Kv * tmp1 * tmp3 / sqsigma );
				}
			} else {
				if (ktype == CV_32F) {
					kernel.at<float>( j + half_height, i + half_width ) =
					(float)( Kv * Kv * tmp1 * tmp2 / sqsigma );
					kernel_2.at<float>( j + half_height, i + half_width ) =
					(float)( Kv * Kv * tmp1 * tmp3 / sqsigma );
				} else {
					kernel.at<double>( j + half_height, i + half_width ) =
					(double)( Kv * Kv * tmp1 * tmp2 / sqsigma );
					kernel_2.at<double>( j + half_height, i + half_width ) =
					(double)( Kv * Kv * tmp1 * tmp3 / sqsigma );
				}
				
			}
		}
	}
	
	if( kernelType == GaborFilter::GABOR_KERNEL_MAG ) {
		magnitude(kernel, kernel_2, kernel_mag);
		gaborKernel = kernel_mag.clone();
	} else {
		gaborKernel = kernel.clone();
	}
}
