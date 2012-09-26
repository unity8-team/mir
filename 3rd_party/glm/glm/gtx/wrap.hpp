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
/// @ref gtx_wrap
/// @file glm/gtx/wrap.hpp
/// @date 2009-11-25 / 2011-06-07
/// @author Christophe Riccio
///
/// @see core (dependence)
///
/// @defgroup gtx_wrap GLM_GTX_wrap: Texture coordinate wrap modes
/// @ingroup gtx
/// 
/// @brief Wrapping mode of texture coordinates.
/// 
/// <glm/gtx/wrap.hpp> need to be included to use these functionalities.
///////////////////////////////////////////////////////////////////////////////////

#ifndef GLM_GTX_wrap
#define GLM_GTX_wrap GLM_VERSION

// Dependency:
#include "../glm.hpp"

#if(defined(GLM_MESSAGES) && !defined(glm_ext))
#	pragma message("GLM: GLM_GTX_wrap extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_wrap
	/// @{

	//! Simulate GL_CLAMP OpenGL wrap mode
	//! From GLM_GTX_wrap extension.
	template <typename genType> 
	genType clamp(genType const & Texcoord);

	//! Simulate GL_REPEAT OpenGL wrap mode
	//! From GLM_GTX_wrap extension.
	template <typename genType> 
	genType repeat(genType const & Texcoord);

	//! Simulate GL_MIRROR_REPEAT OpenGL wrap mode
	//! From GLM_GTX_wrap extension.
	template <typename genType> 
	genType mirrorRepeat(genType const & Texcoord);

	/// @}
}// namespace glm

#include "wrap.inl"

#endif//GLM_GTX_wrap
