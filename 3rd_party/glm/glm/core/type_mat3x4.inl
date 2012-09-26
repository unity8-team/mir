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
/// @ref core
/// @file glm/core/type_mat3x4.inl
/// @date 2006-08-05 / 2011-06-15
/// @author Christophe Riccio
///////////////////////////////////////////////////////////////////////////////////

namespace glm{
namespace detail
{
    template <typename T>
    GLM_FUNC_QUALIFIER typename tmat3x4<T>::size_type tmat3x4<T>::length() const
    {
        return 3;
    }

	template <typename T>
	GLM_FUNC_QUALIFIER typename tmat3x4<T>::size_type tmat3x4<T>::col_size()
	{
		return 4;
	}

	template <typename T>
	GLM_FUNC_QUALIFIER typename tmat3x4<T>::size_type tmat3x4<T>::row_size()
	{
		return 3;
	}

	//////////////////////////////////////
	// Accesses

	template <typename T>
	GLM_FUNC_QUALIFIER typename tmat3x4<T>::col_type & 
	tmat3x4<T>::operator[]
	(
		size_type i
	)
	{
		assert(i < this->length());
		return this->value[i];
	}

	template <typename T>
	GLM_FUNC_QUALIFIER typename tmat3x4<T>::col_type const & 
	tmat3x4<T>::operator[]
	(
		size_type i
	) const
	{
		assert(i < this->length());
		return this->value[i];
	}

    //////////////////////////////////////////////////////////////
    // Constructors

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4()
    {
        this->value[0] = col_type(1, 0, 0, 0);
        this->value[1] = col_type(0, 1, 0, 0);
        this->value[2] = col_type(0, 0, 1, 0);
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		tmat3x4<T> const & m
	)
    {
        this->value[0] = m.value[0];
        this->value[1] = m.value[1];
        this->value[2] = m.value[2];
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		ctor
	)
    {}

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		value_type const & s
	)
    {
		value_type const Zero(0);
        this->value[0] = col_type(s, Zero, Zero, Zero);
        this->value[1] = col_type(Zero, s, Zero, Zero);
        this->value[2] = col_type(Zero, Zero, s, Zero);
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
    (
		value_type const & x0, value_type const & y0, value_type const & z0, value_type const & w0,
        value_type const & x1, value_type const & y1, value_type const & z1, value_type const & w1,
        value_type const & x2, value_type const & y2, value_type const & z2, value_type const & w2
    )
    {
        this->value[0] = col_type(x0, y0, z0, w0);
        this->value[1] = col_type(x1, y1, z1, w1);
        this->value[2] = col_type(x2, y2, z2, w2);
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
    (
        col_type const & v0, 
        col_type const & v1, 
        col_type const & v2
    )
    {
        this->value[0] = v0;
        this->value[1] = v1;
        this->value[2] = v2;
    }

	//////////////////////////////////////
	// Convertion constructors
	template <typename T> 
	template <typename U> 
	GLM_FUNC_DECL tmat3x4<T>::tmat3x4
	(
		U const & s
	)
	{
		value_type const Zero(0);
        this->value[0] = tvec4<T>(value_type(s), Zero, Zero, Zero);
        this->value[1] = tvec4<T>(Zero, value_type(s), Zero, Zero);
        this->value[2] = tvec4<T>(Zero, Zero, value_type(s), Zero);
	}
	
	template <typename T> 
	template <
		typename X1, typename Y1, typename Z1, typename W1, 
		typename X2, typename Y2, typename Z2, typename W2, 
		typename X3, typename Y3, typename Z3, typename W3>  
	GLM_FUNC_DECL tmat3x4<T>::tmat3x4
	(
		X1 const & x1, Y1 const & y1, Z1 const & z1, W1 const & w1, 
		X2 const & x2, Y2 const & y2, Z2 const & z2, W2 const & w2, 
		X3 const & x3, Y3 const & y3, Z3 const & z3, W3 const & w3  
	)		
	{
        this->value[0] = col_type(value_type(x1), value_type(y1), value_type(z1), value_type(w1));
        this->value[1] = col_type(value_type(x2), value_type(y2), value_type(z2), value_type(w2));
        this->value[2] = col_type(value_type(x3), value_type(y3), value_type(z3), value_type(w3));
	}
	
	template <typename T> 
	template <typename V1, typename V2, typename V3> 
	GLM_FUNC_DECL tmat3x4<T>::tmat3x4
	(
		tvec4<V1> const & v1, 
		tvec4<V2> const & v2, 
		tvec4<V3> const & v3
	)		
	{
        this->value[0] = col_type(v1);
        this->value[1] = col_type(v2);
        this->value[2] = col_type(v3);
	}

    // Conversion
    template <typename T> 
    template <typename U> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		tmat3x4<U> const & m
	)
    {
        this->value[0] = col_type(m[0]);
        this->value[1] = col_type(m[1]);
        this->value[2] = col_type(m[2]);
	}

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		tmat2x2<T> const & m
	)
    {
        this->value[0] = col_type(m[0], detail::tvec2<T>(0));
        this->value[1] = col_type(m[1], detail::tvec2<T>(0));
        this->value[2] = col_type(T(0), T(0), T(1), T(0));
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		tmat3x3<T> const & m
	)
    {
        this->value[0] = col_type(m[0], T(0));
        this->value[1] = col_type(m[1], T(0));
        this->value[2] = col_type(m[2], T(0));
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		tmat4x4<T> const & m
	)
    {
        this->value[0] = col_type(m[0]);
        this->value[1] = col_type(m[1]);
        this->value[2] = col_type(m[2]);
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		tmat2x3<T> const & m
	)
    {
        this->value[0] = col_type(m[0], T(0));
        this->value[1] = col_type(m[1], T(0));
        this->value[2] = col_type(T(0), T(0), T(1), T(0));
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		tmat3x2<T> const & m
	)
    {
        this->value[0] = col_type(m[0], detail::tvec2<T>(0));
        this->value[1] = col_type(m[1], detail::tvec2<T>(0));
        this->value[2] = col_type(m[2], T(0), T(1));
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		tmat2x4<T> const & m
	)
    {
        this->value[0] = col_type(m[0]);
        this->value[1] = col_type(m[1]);
        this->value[2] = col_type(T(0), T(0), T(1), T(0));
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		tmat4x2<T> const & m
	)
    {
        this->value[0] = col_type(m[0], detail::tvec2<T>(T(0)));
        this->value[1] = col_type(m[1], detail::tvec2<T>(T(0)));
        this->value[2] = col_type(m[2], detail::tvec2<T>(T(1), T(0)));
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>::tmat3x4
	(
		tmat4x3<T> const & m
	)
    {
        this->value[0] = col_type(m[0], T(0));
        this->value[1] = col_type(m[1], T(0));
        this->value[2] = col_type(m[2], T(0));
    }

    //////////////////////////////////////////////////////////////
    // Unary updatable operators

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>& tmat3x4<T>::operator= 
	(
		tmat3x4<T> const & m
	)
    {
        this->value[0] = m[0];
        this->value[1] = m[1];
        this->value[2] = m[2];
        return *this;
    }

    template <typename T> 
	template <typename U> 
    GLM_FUNC_QUALIFIER tmat3x4<T>& tmat3x4<T>::operator= 
	(
		tmat3x4<U> const & m
	)
    {
        this->value[0] = m[0];
        this->value[1] = m[1];
        this->value[2] = m[2];
        return *this;
    }

    template <typename T> 
	template <typename U> 
    GLM_FUNC_QUALIFIER tmat3x4<T>& tmat3x4<T>::operator+= 
	(
		U const & s
	)
    {
        this->value[0] += s;
        this->value[1] += s;
        this->value[2] += s;
        return *this;
    }

    template <typename T> 
	template <typename U> 
    GLM_FUNC_QUALIFIER tmat3x4<T>& tmat3x4<T>::operator+= 
	(
		tmat3x4<U> const & m
	)
    {
        this->value[0] += m[0];
        this->value[1] += m[1];
        this->value[2] += m[2];
        return *this;
    }

    template <typename T> 
	template <typename U> 
    GLM_FUNC_QUALIFIER tmat3x4<T>& tmat3x4<T>::operator-= 
	(
		U const & s
	)
    {
        this->value[0] -= s;
        this->value[1] -= s;
        this->value[2] -= s;
        return *this;
    }

    template <typename T> 
	template <typename U> 
    GLM_FUNC_QUALIFIER tmat3x4<T>& tmat3x4<T>::operator-= 
	(
		tmat3x4<U> const & m
	)
    {
        this->value[0] -= m[0];
        this->value[1] -= m[1];
        this->value[2] -= m[2];
        return *this;
    }

    template <typename T> 
	template <typename U> 
    GLM_FUNC_QUALIFIER tmat3x4<T>& tmat3x4<T>::operator*= 
	(
		U const & s
	)
    {
        this->value[0] *= s;
        this->value[1] *= s;
        this->value[2] *= s;
        return *this;
    }

    template <typename T> 
	template <typename U> 
    GLM_FUNC_QUALIFIER tmat3x4<T>& tmat3x4<T>::operator*= 
	(
		tmat3x4<U> const & m
	)
    {
        return (*this = tmat3x4<T>(*this * m));
    }

    template <typename T> 
	template <typename U> 
    GLM_FUNC_QUALIFIER tmat3x4<T> & tmat3x4<T>::operator/= 
	(
		U const & s
	)
    {
        this->value[0] /= s;
        this->value[1] /= s;
        this->value[2] /= s;
        return *this;
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>& tmat3x4<T>::operator++ ()
    {
        ++this->value[0];
        ++this->value[1];
        ++this->value[2];
        return *this;
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T>& tmat3x4<T>::operator-- ()
    {
        --this->value[0];
        --this->value[1];
        --this->value[2];
        return *this;
    }

    //////////////////////////////////////////////////////////////
    // Binary operators

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T> operator+ 
	(
		tmat3x4<T> const & m, 
		typename tmat3x4<T>::value_type const & s
	)
    {
        return tmat3x4<T>(
            m[0] + s,
            m[1] + s,
            m[2] + s);
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T> operator+ 
	(
		tmat3x4<T> const & m1, 
		tmat3x4<T> const & m2
	)
    {
        return tmat3x4<T>(
            m1[0] + m2[0],
            m1[1] + m2[1],
            m1[2] + m2[2]);
    }

    template <typename T> 
	GLM_FUNC_QUALIFIER tmat3x4<T> operator- 
	(
		tmat3x4<T> const & m, 
		typename tmat3x4<T>::value_type const & s
	)
    {
        return tmat3x4<T>(
            m[0] - s,
            m[1] - s,
            m[2] - s);
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T> operator- 
	(
		tmat3x4<T> const & m1, 
		tmat3x4<T> const & m2
	)
    {
        return tmat3x4<T>(
            m1[0] - m2[0],
            m1[1] - m2[1],
            m1[2] - m2[2]);
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T> operator* 
	(
		tmat3x4<T> const & m, 
		typename tmat3x4<T>::value_type const & s
	)
    {
        return tmat3x4<T>(
            m[0] * s,
            m[1] * s,
            m[2] * s);
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T> operator* 
	(
		typename tmat3x4<T>::value_type const & s, 
		tmat3x4<T> const & m
	)
    {
        return tmat3x4<T>(
            m[0] * s,
            m[1] * s,
            m[2] * s);
    }
   
    template <typename T>
    GLM_FUNC_QUALIFIER typename tmat3x4<T>::col_type operator* 
	(
		tmat3x4<T> const & m, 
		typename tmat3x4<T>::row_type const & v
	)
    {
        return typename tmat3x4<T>::col_type(
            m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z,
            m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
            m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z,
            m[0][3] * v.x + m[1][3] * v.y + m[2][3] * v.z);
    }

	//			X X X
	//			X X X
	//			X X X
	//			X X X
	// X X X X
    template <typename T> 
    GLM_FUNC_QUALIFIER typename tmat3x4<T>::row_type operator* 
	(
		typename tmat3x4<T>::col_type const & v, 
		tmat3x4<T> const & m
	)
    {
		return typename tmat3x4<T>::row_type(
            v.x * m[0][0] + v.y * m[0][1] + v.z * m[0][2] + v.w * m[0][3],
            v.x * m[1][0] + v.y * m[1][1] + v.z * m[1][2] + v.w * m[1][3],
            v.x * m[2][0] + v.y * m[2][1] + v.z * m[2][2] + v.w * m[2][3]);
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat4x4<T> operator* 
	(
		tmat3x4<T> const & m1, 
		tmat4x3<T> const & m2
	)
    {
        const T SrcA00 = m1[0][0];
        const T SrcA01 = m1[0][1];
        const T SrcA02 = m1[0][2];
        const T SrcA03 = m1[0][3];
        const T SrcA10 = m1[1][0];
        const T SrcA11 = m1[1][1];
        const T SrcA12 = m1[1][2];
        const T SrcA13 = m1[1][3];
        const T SrcA20 = m1[2][0];
        const T SrcA21 = m1[2][1];
        const T SrcA22 = m1[2][2];
        const T SrcA23 = m1[2][3];

        const T SrcB00 = m2[0][0];
        const T SrcB01 = m2[0][1];
        const T SrcB02 = m2[0][2];
        const T SrcB10 = m2[1][0];
        const T SrcB11 = m2[1][1];
        const T SrcB12 = m2[1][2];
        const T SrcB20 = m2[2][0];
        const T SrcB21 = m2[2][1];
        const T SrcB22 = m2[2][2];
        const T SrcB30 = m2[3][0];
        const T SrcB31 = m2[3][1];
        const T SrcB32 = m2[3][2];

        tmat4x4<T> Result(tmat4x4<T>::null);
        Result[0][0] = SrcA00 * SrcB00 + SrcA10 * SrcB01 + SrcA20 * SrcB02;
        Result[0][1] = SrcA01 * SrcB00 + SrcA11 * SrcB01 + SrcA21 * SrcB02;
        Result[0][2] = SrcA02 * SrcB00 + SrcA12 * SrcB01 + SrcA22 * SrcB02;
        Result[0][3] = SrcA03 * SrcB00 + SrcA13 * SrcB01 + SrcA23 * SrcB02;
        Result[1][0] = SrcA00 * SrcB10 + SrcA10 * SrcB11 + SrcA20 * SrcB12;
        Result[1][1] = SrcA01 * SrcB10 + SrcA11 * SrcB11 + SrcA21 * SrcB12;
        Result[1][2] = SrcA02 * SrcB10 + SrcA12 * SrcB11 + SrcA22 * SrcB12;
        Result[1][3] = SrcA03 * SrcB10 + SrcA13 * SrcB11 + SrcA23 * SrcB12;
        Result[2][0] = SrcA00 * SrcB20 + SrcA10 * SrcB21 + SrcA20 * SrcB22;
        Result[2][1] = SrcA01 * SrcB20 + SrcA11 * SrcB21 + SrcA21 * SrcB22;
        Result[2][2] = SrcA02 * SrcB20 + SrcA12 * SrcB21 + SrcA22 * SrcB22;
        Result[2][3] = SrcA03 * SrcB20 + SrcA13 * SrcB21 + SrcA23 * SrcB22;
        Result[3][0] = SrcA00 * SrcB30 + SrcA10 * SrcB31 + SrcA20 * SrcB32;
        Result[3][1] = SrcA01 * SrcB30 + SrcA11 * SrcB31 + SrcA21 * SrcB32;
        Result[3][2] = SrcA02 * SrcB30 + SrcA12 * SrcB31 + SrcA22 * SrcB32;
        Result[3][3] = SrcA03 * SrcB30 + SrcA13 * SrcB31 + SrcA23 * SrcB32;
        return Result;
    }

	template <typename T>
	GLM_FUNC_QUALIFIER tmat2x4<T> operator* 
	(
		tmat3x4<T> const & m1, 
		tmat2x3<T> const & m2
	)
	{
		return tmat2x4<T>(
			m1[0][0] * m2[0][0] + m1[1][0] * m2[0][1] + m1[2][0] * m2[0][2],
			m1[0][1] * m2[0][0] + m1[1][1] * m2[0][1] + m1[2][1] * m2[0][2],
			m1[0][2] * m2[0][0] + m1[1][2] * m2[0][1] + m1[2][2] * m2[0][2],
			m1[0][3] * m2[0][0] + m1[1][3] * m2[0][1] + m1[2][3] * m2[0][2],
			m1[0][0] * m2[1][0] + m1[1][0] * m2[1][1] + m1[2][0] * m2[1][2],
			m1[0][1] * m2[1][0] + m1[1][1] * m2[1][1] + m1[2][1] * m2[1][2],
			m1[0][2] * m2[1][0] + m1[1][2] * m2[1][1] + m1[2][2] * m2[1][2],
			m1[0][3] * m2[1][0] + m1[1][3] * m2[1][1] + m1[2][3] * m2[1][2]);
	}

	template <typename T>
	GLM_FUNC_QUALIFIER tmat3x4<T> operator* 
	(
		tmat3x4<T> const & m1, 
		tmat3x3<T> const & m2
	)
	{
		return tmat3x4<T>(
			m1[0][0] * m2[0][0] + m1[1][0] * m2[0][1] + m1[2][0] * m2[0][2],
			m1[0][1] * m2[0][0] + m1[1][1] * m2[0][1] + m1[2][1] * m2[0][2],
			m1[0][2] * m2[0][0] + m1[1][2] * m2[0][1] + m1[2][2] * m2[0][2],
			m1[0][3] * m2[0][0] + m1[1][3] * m2[0][1] + m1[2][3] * m2[0][2],
			m1[0][0] * m2[1][0] + m1[1][0] * m2[1][1] + m1[2][0] * m2[1][2],
			m1[0][1] * m2[1][0] + m1[1][1] * m2[1][1] + m1[2][1] * m2[1][2],
			m1[0][2] * m2[1][0] + m1[1][2] * m2[1][1] + m1[2][2] * m2[1][2],
			m1[0][3] * m2[1][0] + m1[1][3] * m2[1][1] + m1[2][3] * m2[1][2],
			m1[0][0] * m2[2][0] + m1[1][0] * m2[2][1] + m1[2][0] * m2[2][2],
			m1[0][1] * m2[2][0] + m1[1][1] * m2[2][1] + m1[2][1] * m2[2][2],
			m1[0][2] * m2[2][0] + m1[1][2] * m2[2][1] + m1[2][2] * m2[2][2],
			m1[0][3] * m2[2][0] + m1[1][3] * m2[2][1] + m1[2][3] * m2[2][2]);
	}

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T> operator/ 
	(
		tmat3x4<T> const & m, 
		typename tmat3x4<T>::value_type const & s
	)
    {
        return tmat3x4<T>(
            m[0] / s,
            m[1] / s,
            m[2] / s);        
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T> operator/
	(
		typename tmat3x4<T>::value_type const & s, 
		tmat3x4<T> const & m
	)
    {
        return tmat3x4<T>(
            s / m[0],
            s / m[1],
            s / m[2]);        
    }

	// Unary constant operators
    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T> const operator- 
	(
		tmat3x4<T> const & m
	)
    {
        return tmat3x4<T>(
            -m[0], 
            -m[1],
            -m[2]);
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T> const operator++ 
	(
		tmat3x4<T> const & m, 
		int
	) 
    {
        return tmat3x4<T>(
            m[0] + T(1),
            m[1] + T(1),
            m[2] + T(1));
    }

    template <typename T> 
    GLM_FUNC_QUALIFIER tmat3x4<T> const operator-- 
	(
		tmat3x4<T> const & m, 
		int
	) 
    {
        return tmat3x4<T>(
            m[0] - T(1),
            m[1] - T(1),
            m[2] - T(1));
	}

	//////////////////////////////////////
	// Boolean operators

	template <typename T> 
	GLM_FUNC_QUALIFIER bool operator==
	(
		tmat3x4<T> const & m1, 
		tmat3x4<T> const & m2
	)
	{
		return (m1[0] == m2[0]) && (m1[1] == m2[1]) && (m1[2] == m2[2]);
	}

	template <typename T> 
	GLM_FUNC_QUALIFIER bool operator!=
	(
		tmat3x4<T> const & m1, 
		tmat3x4<T> const & m2
	)
	{
		return (m1[0] != m2[0]) || (m1[1] != m2[1]) || (m1[2] != m2[2]);
	}

} //namespace detail
} //namespace glm
