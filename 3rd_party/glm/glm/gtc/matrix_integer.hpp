///////////////////////////////////////////////////////////////////////////////////
/// OpenGL Mathematics (glm.g-truc.net)
///
/// Copyright (c) 2005 - 2012 G-Truc Creation (www.g-truc.net)
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
/// 
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
/// 
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.
///
/// @ref gtc_matrix_integer
/// @file glm/gtc/matrix_integer.hpp
/// @date 2011-01-20 / 2011-06-05
/// @author Christophe Riccio
///
/// @see core (dependence)
///
/// @defgroup gtc_matrix_integer GLM_GTC_matrix_integer: Integer matrix types
/// @ingroup gtc
/// 
/// Defines a number of matrices with integer types.
/// <glm/gtc/matrix_integer.hpp> need to be included to use these functionalities.
///////////////////////////////////////////////////////////////////////////////////

#ifndef GLM_GTC_matrix_integer
#define GLM_GTC_matrix_integer GLM_VERSION

// Dependency:
#include "../glm.hpp"

#if(defined(GLM_MESSAGES) && !defined(glm_ext))
#	pragma message("GLM: GLM_GTC_matrix_integer extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_matrix_integer
	/// @{

	/// High-precision signed integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<highp_int>				highp_imat2;	

	/// High-precision signed integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<highp_int>				highp_imat3;

	/// High-precision signed integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<highp_int>				highp_imat4;

	/// High-precision signed integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<highp_int>				highp_imat2x2;

	/// High-precision signed integer 2x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x3<highp_int>				highp_imat2x3;

	/// High-precision signed integer 2x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x4<highp_int>				highp_imat2x4;

	/// High-precision signed integer 3x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x2<highp_int>				highp_imat3x2;

	/// High-precision signed integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<highp_int>				highp_imat3x3;

	/// High-precision signed integer 3x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x4<highp_int>				highp_imat3x4;

	/// High-precision signed integer 4x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x2<highp_int>				highp_imat4x2;

	/// High-precision signed integer 4x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x3<highp_int>				highp_imat4x3;

	/// High-precision signed integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<highp_int>				highp_imat4x4;


	/// Medium-precision signed integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<mediump_int>			mediump_imat2;

	/// Medium-precision signed integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<mediump_int>			mediump_imat3;

	/// Medium-precision signed integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<mediump_int>			mediump_imat4;


	/// Medium-precision signed integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<mediump_int>			mediump_imat2x2;

	/// Medium-precision signed integer 2x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x3<mediump_int>			mediump_imat2x3;

	/// Medium-precision signed integer 2x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x4<mediump_int>			mediump_imat2x4;

	/// Medium-precision signed integer 3x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x2<mediump_int>			mediump_imat3x2;

	/// Medium-precision signed integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<mediump_int>			mediump_imat3x3;

	/// Medium-precision signed integer 3x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x4<mediump_int>			mediump_imat3x4;

	/// Medium-precision signed integer 4x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x2<mediump_int>			mediump_imat4x2;

	/// Medium-precision signed integer 4x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x3<mediump_int>			mediump_imat4x3;

	/// Medium-precision signed integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<mediump_int>			mediump_imat4x4;


	/// Low-precision signed integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<lowp_int>				lowp_imat2;
	
	/// Low-precision signed integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<lowp_int>				lowp_imat3;

	/// Low-precision signed integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<lowp_int>				lowp_imat4;


	/// Low-precision signed integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<lowp_int>				lowp_imat2x2;

	/// Low-precision signed integer 2x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x3<lowp_int>				lowp_imat2x3;

	/// Low-precision signed integer 2x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x4<lowp_int>				lowp_imat2x4;

	/// Low-precision signed integer 3x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x2<lowp_int>				lowp_imat3x2;

	/// Low-precision signed integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<lowp_int>				lowp_imat3x3;

	/// Low-precision signed integer 3x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x4<lowp_int>				lowp_imat3x4;

	/// Low-precision signed integer 4x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x2<lowp_int>				lowp_imat4x2;

	/// Low-precision signed integer 4x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x3<lowp_int>				lowp_imat4x3;

	/// Low-precision signed integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<lowp_int>				lowp_imat4x4;


	/// High-precision unsigned integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<highp_uint>				highp_umat2;	

	/// High-precision unsigned integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<highp_uint>				highp_umat3;

	/// High-precision unsigned integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<highp_uint>				highp_umat4;

	/// High-precision unsigned integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<highp_uint>				highp_umat2x2;

	/// High-precision unsigned integer 2x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x3<highp_uint>				highp_umat2x3;

	/// High-precision unsigned integer 2x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x4<highp_uint>				highp_umat2x4;

	/// High-precision unsigned integer 3x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x2<highp_uint>				highp_umat3x2;

	/// High-precision unsigned integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<highp_uint>				highp_umat3x3;

	/// High-precision unsigned integer 3x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x4<highp_uint>				highp_umat3x4;

	/// High-precision unsigned integer 4x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x2<highp_uint>				highp_umat4x2;

	/// High-precision unsigned integer 4x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x3<highp_uint>				highp_umat4x3;

	/// High-precision unsigned integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<highp_uint>				highp_umat4x4;


	/// Medium-precision unsigned integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<mediump_uint>			mediump_umat2;

	/// Medium-precision unsigned integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<mediump_uint>			mediump_umat3;

	/// Medium-precision unsigned integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<mediump_uint>			mediump_umat4;


	/// Medium-precision unsigned integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<mediump_uint>			mediump_umat2x2;

	/// Medium-precision unsigned integer 2x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x3<mediump_uint>			mediump_umat2x3;

	/// Medium-precision unsigned integer 2x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x4<mediump_uint>			mediump_umat2x4;

	/// Medium-precision unsigned integer 3x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x2<mediump_uint>			mediump_umat3x2;

	/// Medium-precision unsigned integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<mediump_uint>			mediump_umat3x3;

	/// Medium-precision unsigned integer 3x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x4<mediump_uint>			mediump_umat3x4;

	/// Medium-precision unsigned integer 4x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x2<mediump_uint>			mediump_umat4x2;

	/// Medium-precision unsigned integer 4x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x3<mediump_uint>			mediump_umat4x3;

	/// Medium-precision unsigned integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<mediump_uint>			mediump_umat4x4;


	/// Low-precision unsigned integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<lowp_uint>				lowp_umat2;
	
	/// Low-precision unsigned integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<lowp_uint>				lowp_umat3;

	/// Low-precision unsigned integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<lowp_uint>				lowp_umat4;


	/// Low-precision unsigned integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x2<lowp_uint>				lowp_umat2x2;

	/// Low-precision unsigned integer 2x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x3<lowp_uint>				lowp_umat2x3;

	/// Low-precision unsigned integer 2x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat2x4<lowp_uint>				lowp_umat2x4;

	/// Low-precision unsigned integer 3x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x2<lowp_uint>				lowp_umat3x2;

	/// Low-precision unsigned integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x3<lowp_uint>				lowp_umat3x3;

	/// Low-precision unsigned integer 3x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat3x4<lowp_uint>				lowp_umat3x4;

	/// Low-precision unsigned integer 4x2 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x2<lowp_uint>				lowp_umat4x2;

	/// Low-precision unsigned integer 4x3 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x3<lowp_uint>				lowp_umat4x3;

	/// Low-precision unsigned integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef detail::tmat4x4<lowp_uint>				lowp_umat4x4;

#if(defined(GLM_PRECISION_HIGHP_INT))
	typedef highp_imat2								imat2;
	typedef highp_imat3								imat3;
	typedef highp_imat4								imat4;
	typedef highp_imat2x2							imat2x2;
	typedef highp_imat2x3							imat2x3;
	typedef highp_imat2x4							imat2x4;
	typedef highp_imat3x2							imat3x2;
	typedef highp_imat3x3							imat3x3;
	typedef highp_imat3x4							imat3x4;
	typedef highp_imat4x2							imat4x2;
	typedef highp_imat4x3							imat4x3;
	typedef highp_imat4x4							imat4x4;
#elif(defined(GLM_PRECISION_LOWP_INT))
	typedef lowp_imat2								imat2;
	typedef lowp_imat3								imat3;
	typedef lowp_imat4								imat4;
	typedef lowp_imat2x2							imat2x2;
	typedef lowp_imat2x3							imat2x3;
	typedef lowp_imat2x4							imat2x4;
	typedef lowp_imat3x2							imat3x2;
	typedef lowp_imat3x3							imat3x3;
	typedef lowp_imat3x4							imat3x4;
	typedef lowp_imat4x2							imat4x2;
	typedef lowp_imat4x3							imat4x3;
	typedef lowp_imat4x4							imat4x4;
#else //if(defined(GLM_PRECISION_MEDIUMP_INT))

	/// Signed integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat2							imat2;

	/// Signed integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat3							imat3;

	/// Signed integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat4							imat4;

	/// Signed integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat2x2							imat2x2;

	/// Signed integer 2x3 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat2x3							imat2x3;

	/// Signed integer 2x4 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat2x4							imat2x4;

	/// Signed integer 3x2 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat3x2							imat3x2;

	/// Signed integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat3x3							imat3x3;

	/// Signed integer 3x4 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat3x4							imat3x4;

	/// Signed integer 4x2 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat4x2							imat4x2;

	/// Signed integer 4x3 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat4x3							imat4x3;

	/// Signed integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_imat4x4							imat4x4;
#endif//GLM_PRECISION

#if(defined(GLM_PRECISION_HIGHP_UINT))
	typedef highp_umat2								umat2;
	typedef highp_umat3								umat3;
	typedef highp_umat4								umat4;
	typedef highp_umat2x2							umat2x2;
	typedef highp_umat2x3							umat2x3;
	typedef highp_umat2x4							umat2x4;
	typedef highp_umat3x2							umat3x2;
	typedef highp_umat3x3							umat3x3;
	typedef highp_umat3x4							umat3x4;
	typedef highp_umat4x2							umat4x2;
	typedef highp_umat4x3							umat4x3;
	typedef highp_umat4x4							umat4x4;
#elif(defined(GLM_PRECISION_LOWP_UINT))
	typedef lowp_umat2								umat2;
	typedef lowp_umat3								umat3;
	typedef lowp_umat4								umat4;
	typedef lowp_umat2x2							umat2x2;
	typedef lowp_umat2x3							umat2x3;
	typedef lowp_umat2x4							umat2x4;
	typedef lowp_umat3x2							umat3x2;
	typedef lowp_umat3x3							umat3x3;
	typedef lowp_umat3x4							umat3x4;
	typedef lowp_umat4x2							umat4x2;
	typedef lowp_umat4x3							umat4x3;
	typedef lowp_umat4x4							umat4x4;
#else //if(defined(GLM_PRECISION_MEDIUMP_UINT))
	
    /// Unsigned integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat2							umat2;

	/// Unsigned integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat3							umat3;

	/// Unsigned integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat4							umat4;

	/// Unsigned integer 2x2 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat2x2							umat2x2;

	/// Unsigned integer 2x3 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat2x3							umat2x3;

	/// Unsigned integer 2x4 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat2x4							umat2x4;

	/// Unsigned integer 3x2 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat3x2							umat3x2;

	/// Unsigned integer 3x3 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat3x3							umat3x3;

	/// Unsigned integer 3x4 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat3x4							umat3x4;

	/// Unsigned integer 4x2 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat4x2							umat4x2;

	/// Unsigned integer 4x3 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat4x3							umat4x3;

	/// Unsigned integer 4x4 matrix.
	/// @see gtc_matrix_integer
	typedef mediump_umat4x4							umat4x4;
#endif//GLM_PRECISION

	/// @}
}//namespace glm

#endif//GLM_GTC_matrix_integer
