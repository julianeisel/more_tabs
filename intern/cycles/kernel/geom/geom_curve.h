/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

#ifdef __HAIR__

/* curve attributes */

ccl_device float curve_attribute_float(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float *dx, float *dy)
{
	if(elem == ATTR_ELEMENT_CURVE) {
#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = 0.0f;
		if(dy) *dy = 0.0f;
#endif

		return kernel_tex_fetch(__attributes_float, offset + sd->prim);
	}
	else if(elem == ATTR_ELEMENT_CURVE_KEY || elem == ATTR_ELEMENT_CURVE_KEY_MOTION) {
		float4 curvedata = kernel_tex_fetch(__curves, sd->prim);
		int k0 = __float_as_int(curvedata.x) + PRIMITIVE_UNPACK_SEGMENT(sd->type);
		int k1 = k0 + 1;

		float f0 = kernel_tex_fetch(__attributes_float, offset + k0);
		float f1 = kernel_tex_fetch(__attributes_float, offset + k1);

#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = sd->du.dx*(f1 - f0);
		if(dy) *dy = 0.0f;
#endif

		return (1.0f - sd->u)*f0 + sd->u*f1;
	}
	else {
#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = 0.0f;
		if(dy) *dy = 0.0f;
#endif

		return 0.0f;
	}
}

ccl_device float3 curve_attribute_float3(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float3 *dx, float3 *dy)
{
	if(elem == ATTR_ELEMENT_CURVE) {
		/* idea: we can't derive any useful differentials here, but for tiled
		 * mipmap image caching it would be useful to avoid reading the highest
		 * detail level always. maybe a derivative based on the hair density
		 * could be computed somehow? */
#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = make_float3(0.0f, 0.0f, 0.0f);
		if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);
#endif

		return float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + sd->prim));
	}
	else if(elem == ATTR_ELEMENT_CURVE_KEY || elem == ATTR_ELEMENT_CURVE_KEY_MOTION) {
		float4 curvedata = kernel_tex_fetch(__curves, sd->prim);
		int k0 = __float_as_int(curvedata.x) + PRIMITIVE_UNPACK_SEGMENT(sd->type);
		int k1 = k0 + 1;

		float3 f0 = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + k0));
		float3 f1 = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + k1));

#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = sd->du.dx*(f1 - f0);
		if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);
#endif

		return (1.0f - sd->u)*f0 + sd->u*f1;
	}
	else {
#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = make_float3(0.0f, 0.0f, 0.0f);
		if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);
#endif

		return make_float3(0.0f, 0.0f, 0.0f);
	}
}

/* hair info node functions */

ccl_device float curve_thickness(KernelGlobals *kg, ShaderData *sd)
{
	float r = 0.0f;

	if(sd->type & PRIMITIVE_ALL_CURVE) {
		float4 curvedata = kernel_tex_fetch(__curves, sd->prim);
		int k0 = __float_as_int(curvedata.x) + PRIMITIVE_UNPACK_SEGMENT(sd->type);
		int k1 = k0 + 1;

		float4 P_curve[2];

		if(sd->type & PRIMITIVE_CURVE) {
			P_curve[0]= kernel_tex_fetch(__curve_keys, k0);
			P_curve[1]= kernel_tex_fetch(__curve_keys, k1);
		}
		else {
			motion_curve_keys(kg, sd->object, sd->prim, sd->time, k0, k1, P_curve);
		}

		r = (P_curve[1].w - P_curve[0].w) * sd->u + P_curve[0].w;
	}

	return r*2.0f;
}

ccl_device float3 curve_tangent_normal(KernelGlobals *kg, ShaderData *sd)
{	
	float3 tgN = make_float3(0.0f,0.0f,0.0f);

	if(sd->type & PRIMITIVE_ALL_CURVE) {

		tgN = -(-sd->I - sd->dPdu * (dot(sd->dPdu,-sd->I) / len_squared(sd->dPdu)));
		tgN = normalize(tgN);

		/* need to find suitable scaled gd for corrected normal */
#if 0
		tgN = normalize(tgN - gd * sd->dPdu);
#endif
	}

	return tgN;
}

#endif

#ifdef __HAIR__
ccl_device_inline void curvebounds(float *lower, float *upper, float *extremta, float *extrema, float *extremtb, float *extremb, float p0, float p1, float p2, float p3)
{
	float halfdiscroot = (p2 * p2 - 3 * p3 * p1);
	float ta = -1.0f;
	float tb = -1.0f;
	*extremta = -1.0f;
	*extremtb = -1.0f;
	*upper = p0;
	*lower = p0 + p1 + p2 + p3;
	*extrema = *upper;
	*extremb = *lower;
	if(*lower >= *upper) {
		*upper = *lower;
		*lower = p0;
	}

	if(halfdiscroot >= 0) {
		halfdiscroot = sqrt(halfdiscroot);
		ta = (-p2 - halfdiscroot) / (3 * p3);
		tb = (-p2 + halfdiscroot) / (3 * p3);
	}

	float t2;
	float t3;
	if(ta > 0.0f && ta < 1.0f) {
		t2 = ta * ta;
		t3 = t2 * ta;
		*extremta = ta;
		*extrema = p3 * t3 + p2 * t2 + p1 * ta + p0;
		if(*extrema > *upper) {
			*upper = *extrema;
		}
		if(*extrema < *lower) {
			*lower = *extrema;
		}
	}
	if(tb > 0.0f && tb < 1.0f) {
		t2 = tb * tb;
		t3 = t2 * tb;
		*extremtb = tb;
		*extremb = p3 * t3 + p2 * t2 + p1 * tb + p0;
		if(*extremb >= *upper) {
			*upper = *extremb;
		}
		if(*extremb <= *lower) {
			*lower = *extremb;
		}
	}
}

#ifdef __KERNEL_SSE2__
ccl_device_inline __m128 transform_point_T3(const __m128 t[3], const __m128 &a)
{
	return fma(broadcast<0>(a), t[0], fma(broadcast<1>(a), t[1], _mm_mul_ps(broadcast<2>(a), t[2])));
}
#endif

#ifdef __KERNEL_SSE2__
/* Pass P and idir by reference to aligned vector */
ccl_device_inline bool bvh_cardinal_curve_intersect(KernelGlobals *kg, Intersection *isect,
	const float3 &P, const float3 &idir, uint visibility, int object, int curveAddr, float time, int type, uint *lcg_state, float difl, float extmax)
#else
ccl_device_inline bool bvh_cardinal_curve_intersect(KernelGlobals *kg, Intersection *isect,
	float3 P, float3 idir, uint visibility, int object, int curveAddr, float time,int type, uint *lcg_state, float difl, float extmax)
#endif
{
	int segment = PRIMITIVE_UNPACK_SEGMENT(type);
	float epsilon = 0.0f;
	float r_st, r_en;

	int depth = kernel_data.curve.subdivisions;
	int flags = kernel_data.curve.curveflags;
	int prim = kernel_tex_fetch(__prim_index, curveAddr);

#ifdef __KERNEL_SSE2__
	__m128 vdir = _mm_div_ps(_mm_set1_ps(1.0f), load_m128(idir));
	__m128 vcurve_coef[4];
	const float3 *curve_coef = (float3 *)vcurve_coef;
	
	{
		__m128 dtmp = _mm_mul_ps(vdir, vdir);
		__m128 d_ss = _mm_sqrt_ss(_mm_add_ss(dtmp, broadcast<2>(dtmp)));
		__m128 rd_ss = _mm_div_ss(_mm_set_ss(1.0f), d_ss);

		__m128i v00vec = _mm_load_si128((__m128i *)&kg->__curves.data[prim]);
		int2 &v00 = (int2 &)v00vec;

		int k0 = v00.x + segment;
		int k1 = k0 + 1;
		int ka = max(k0 - 1, v00.x);
		int kb = min(k1 + 1, v00.x + v00.y - 1);

		__m128 P_curve[4];

		if(type & PRIMITIVE_CURVE) {
			P_curve[0] = _mm_load_ps(&kg->__curve_keys.data[ka].x);
			P_curve[1] = _mm_load_ps(&kg->__curve_keys.data[k0].x);
			P_curve[2] = _mm_load_ps(&kg->__curve_keys.data[k1].x);
			P_curve[3] = _mm_load_ps(&kg->__curve_keys.data[kb].x);
		}
		else {
			int fobject = (object == OBJECT_NONE)? kernel_tex_fetch(__prim_object, curveAddr): object;
			motion_cardinal_curve_keys(kg, fobject, prim, time, ka, k0, k1, kb, (float4*)&P_curve);
		}

		__m128 rd_sgn = set_sign_bit<0, 1, 1, 1>(broadcast<0>(rd_ss));
		__m128 mul_zxxy = _mm_mul_ps(shuffle<2, 0, 0, 1>(vdir), rd_sgn);
		__m128 mul_yz = _mm_mul_ps(shuffle<1, 2, 1, 2>(vdir), mul_zxxy);
		__m128 mul_shuf = shuffle<0, 1, 2, 3>(mul_zxxy, mul_yz);
		__m128 vdir0 = _mm_and_ps(vdir, _mm_castsi128_ps(_mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0)));

		__m128 htfm0 = shuffle<0, 2, 0, 3>(mul_shuf, vdir0);
		__m128 htfm1 = shuffle<1, 0, 1, 3>(_mm_set_ss(_mm_cvtss_f32(d_ss)), vdir0);
		__m128 htfm2 = shuffle<1, 3, 2, 3>(mul_shuf, vdir0);

		__m128 htfm[] = { htfm0, htfm1, htfm2 };
		__m128 vP = load_m128(P);
		__m128 p0 = transform_point_T3(htfm, _mm_sub_ps(P_curve[0], vP));
		__m128 p1 = transform_point_T3(htfm, _mm_sub_ps(P_curve[1], vP));
		__m128 p2 = transform_point_T3(htfm, _mm_sub_ps(P_curve[2], vP));
		__m128 p3 = transform_point_T3(htfm, _mm_sub_ps(P_curve[3], vP));

		float fc = 0.71f;
		__m128 vfc = _mm_set1_ps(fc);
		__m128 vfcxp3 = _mm_mul_ps(vfc, p3);

		vcurve_coef[0] = p1;
		vcurve_coef[1] = _mm_mul_ps(vfc, _mm_sub_ps(p2, p0));
		vcurve_coef[2] = fma(_mm_set1_ps(fc * 2.0f), p0, fma(_mm_set1_ps(fc - 3.0f), p1, fms(_mm_set1_ps(3.0f - 2.0f * fc), p2, vfcxp3)));
		vcurve_coef[3] = fms(_mm_set1_ps(fc - 2.0f), _mm_sub_ps(p2, p1), fms(vfc, p0, vfcxp3));

		r_st = ((float4 &)P_curve[1]).w;
		r_en = ((float4 &)P_curve[2]).w;
	}
#else
	float3 curve_coef[4];

	/* curve Intersection check */
	float3 dir = 1.0f/idir;

	/* obtain curve parameters */
	{
		/* ray transform created - this should be created at beginning of intersection loop */
		Transform htfm;
		float d = sqrtf(dir.x * dir.x + dir.z * dir.z);
		htfm = make_transform(
			dir.z / d, 0, -dir.x /d, 0,
			-dir.x * dir.y /d, d, -dir.y * dir.z /d, 0,
			dir.x, dir.y, dir.z, 0,
			0, 0, 0, 1);

		float4 v00 = kernel_tex_fetch(__curves, prim);

		int k0 = __float_as_int(v00.x) + segment;
		int k1 = k0 + 1;

		int ka = max(k0 - 1,__float_as_int(v00.x));
		int kb = min(k1 + 1,__float_as_int(v00.x) + __float_as_int(v00.y) - 1);

		float4 P_curve[4];

		if(type & PRIMITIVE_CURVE) {
			P_curve[0] = kernel_tex_fetch(__curve_keys, ka);
			P_curve[1] = kernel_tex_fetch(__curve_keys, k0);
			P_curve[2] = kernel_tex_fetch(__curve_keys, k1);
			P_curve[3] = kernel_tex_fetch(__curve_keys, kb);
		}
		else {
			int fobject = (object == OBJECT_NONE)? kernel_tex_fetch(__prim_object, curveAddr): object;
			motion_cardinal_curve_keys(kg, fobject, prim, time, ka, k0, k1, kb, P_curve);
		}

		float3 p0 = transform_point(&htfm, float4_to_float3(P_curve[0]) - P);
		float3 p1 = transform_point(&htfm, float4_to_float3(P_curve[1]) - P);
		float3 p2 = transform_point(&htfm, float4_to_float3(P_curve[2]) - P);
		float3 p3 = transform_point(&htfm, float4_to_float3(P_curve[3]) - P);

		float fc = 0.71f;
		curve_coef[0] = p1;
		curve_coef[1] = -fc*p0 + fc*p2;
		curve_coef[2] = 2.0f * fc * p0 + (fc - 3.0f) * p1 + (3.0f - 2.0f * fc) * p2 - fc * p3;
		curve_coef[3] = -fc * p0 + (2.0f - fc) * p1 + (fc - 2.0f) * p2 + fc * p3;
		r_st = P_curve[1].w;
		r_en = P_curve[2].w;
	}
#endif

	float r_curr = max(r_st, r_en);

	if((flags & CURVE_KN_RIBBONS) || !(flags & CURVE_KN_BACKFACING))
		epsilon = 2 * r_curr;

	/* find bounds - this is slow for cubic curves */
	float upper, lower;

	float zextrem[4];
	curvebounds(&lower, &upper, &zextrem[0], &zextrem[1], &zextrem[2], &zextrem[3], curve_coef[0].z, curve_coef[1].z, curve_coef[2].z, curve_coef[3].z);
	if(lower - r_curr > isect->t || upper + r_curr < epsilon)
		return false;

	/* minimum width extension */
	float mw_extension = min(difl * fabsf(upper), extmax);
	float r_ext = mw_extension + r_curr;

	float xextrem[4];
	curvebounds(&lower, &upper, &xextrem[0], &xextrem[1], &xextrem[2], &xextrem[3], curve_coef[0].x, curve_coef[1].x, curve_coef[2].x, curve_coef[3].x);
	if(lower > r_ext || upper < -r_ext)
		return false;

	float yextrem[4];
	curvebounds(&lower, &upper, &yextrem[0], &yextrem[1], &yextrem[2], &yextrem[3], curve_coef[0].y, curve_coef[1].y, curve_coef[2].y, curve_coef[3].y);
	if(lower > r_ext || upper < -r_ext)
		return false;

	/* setup recurrent loop */
	int level = 1 << depth;
	int tree = 0;
	float resol = 1.0f / (float)level;
	bool hit = false;

	/* begin loop */
	while(!(tree >> (depth))) {
		float i_st = tree * resol;
		float i_en = i_st + (level * resol);
#ifdef __KERNEL_SSE2__
		__m128 vi_st = _mm_set1_ps(i_st), vi_en = _mm_set1_ps(i_en);
		__m128 vp_st = fma(fma(fma(vcurve_coef[3], vi_st, vcurve_coef[2]), vi_st, vcurve_coef[1]), vi_st, vcurve_coef[0]);
		__m128 vp_en = fma(fma(fma(vcurve_coef[3], vi_en, vcurve_coef[2]), vi_en, vcurve_coef[1]), vi_en, vcurve_coef[0]);

		__m128 vbmin = _mm_min_ps(vp_st, vp_en);
		__m128 vbmax = _mm_max_ps(vp_st, vp_en);

		float3 &bmin = (float3 &)vbmin, &bmax = (float3 &)vbmax;
		float &bminx = bmin.x, &bminy = bmin.y, &bminz = bmin.z;
		float &bmaxx = bmax.x, &bmaxy = bmax.y, &bmaxz = bmax.z;
		float3 &p_st = (float3 &)vp_st, &p_en = (float3 &)vp_en;
#else
		float3 p_st = ((curve_coef[3] * i_st + curve_coef[2]) * i_st + curve_coef[1]) * i_st + curve_coef[0];
		float3 p_en = ((curve_coef[3] * i_en + curve_coef[2]) * i_en + curve_coef[1]) * i_en + curve_coef[0];
		
		float bminx = min(p_st.x, p_en.x);
		float bmaxx = max(p_st.x, p_en.x);
		float bminy = min(p_st.y, p_en.y);
		float bmaxy = max(p_st.y, p_en.y);
		float bminz = min(p_st.z, p_en.z);
		float bmaxz = max(p_st.z, p_en.z);
#endif

		if(xextrem[0] >= i_st && xextrem[0] <= i_en) {
			bminx = min(bminx,xextrem[1]);
			bmaxx = max(bmaxx,xextrem[1]);
		}
		if(xextrem[2] >= i_st && xextrem[2] <= i_en) {
			bminx = min(bminx,xextrem[3]);
			bmaxx = max(bmaxx,xextrem[3]);
		}
		if(yextrem[0] >= i_st && yextrem[0] <= i_en) {
			bminy = min(bminy,yextrem[1]);
			bmaxy = max(bmaxy,yextrem[1]);
		}
		if(yextrem[2] >= i_st && yextrem[2] <= i_en) {
			bminy = min(bminy,yextrem[3]);
			bmaxy = max(bmaxy,yextrem[3]);
		}
		if(zextrem[0] >= i_st && zextrem[0] <= i_en) {
			bminz = min(bminz,zextrem[1]);
			bmaxz = max(bmaxz,zextrem[1]);
		}
		if(zextrem[2] >= i_st && zextrem[2] <= i_en) {
			bminz = min(bminz,zextrem[3]);
			bmaxz = max(bmaxz,zextrem[3]);
		}

		float r1 = r_st + (r_en - r_st) * i_st;
		float r2 = r_st + (r_en - r_st) * i_en;
		r_curr = max(r1, r2);

		mw_extension = min(difl * fabsf(bmaxz), extmax);
		float r_ext = mw_extension + r_curr;
		float coverage = 1.0f;

		if (bminz - r_curr > isect->t || bmaxz + r_curr < epsilon || bminx > r_ext|| bmaxx < -r_ext|| bminy > r_ext|| bmaxy < -r_ext) {
			/* the bounding box does not overlap the square centered at O */
			tree += level;
			level = tree & -tree;
		}
		else if (level == 1) {

			/* the maximum recursion depth is reached.
			* check if dP0.(Q-P0)>=0 and dPn.(Pn-Q)>=0.
			* dP* is reversed if necessary.*/
			float t = isect->t;
			float u = 0.0f;
			if(flags & CURVE_KN_RIBBONS) {
				float3 tg = (p_en - p_st);
				float w = tg.x * tg.x + tg.y * tg.y;
				if (w == 0) {
					tree++;
					level = tree & -tree;
					continue;
				}
				w = -(p_st.x * tg.x + p_st.y * tg.y) / w;
				w = clamp((float)w, 0.0f, 1.0f);

				/* compute u on the curve segment */
				u = i_st * (1 - w) + i_en * w;
				r_curr = r_st + (r_en - r_st) * u;
				/* compare x-y distances */
				float3 p_curr = ((curve_coef[3] * u + curve_coef[2]) * u + curve_coef[1]) * u + curve_coef[0];

				float3 dp_st = (3 * curve_coef[3] * i_st + 2 * curve_coef[2]) * i_st + curve_coef[1];
				if (dot(tg, dp_st)< 0)
					dp_st *= -1;
				if (dot(dp_st, -p_st) + p_curr.z * dp_st.z < 0) {
					tree++;
					level = tree & -tree;
					continue;
				}
				float3 dp_en = (3 * curve_coef[3] * i_en + 2 * curve_coef[2]) * i_en + curve_coef[1];
				if (dot(tg, dp_en) < 0)
					dp_en *= -1;
				if (dot(dp_en, p_en) - p_curr.z * dp_en.z < 0) {
					tree++;
					level = tree & -tree;
					continue;
				}

				/* compute coverage */
				float r_ext = r_curr;
				coverage = 1.0f;
				if(difl != 0.0f) {
					mw_extension = min(difl * fabsf(bmaxz), extmax);
					r_ext = mw_extension + r_curr;
					float d = sqrtf(p_curr.x * p_curr.x + p_curr.y * p_curr.y);
					float d0 = d - r_curr;
					float d1 = d + r_curr;
					if (d0 >= 0)
						coverage = (min(d1 / mw_extension, 1.0f) - min(d0 / mw_extension, 1.0f)) * 0.5f;
					else // inside
						coverage = (min(d1 / mw_extension, 1.0f) + min(-d0 / mw_extension, 1.0f)) * 0.5f;
				}
				
				if (p_curr.x * p_curr.x + p_curr.y * p_curr.y >= r_ext * r_ext || p_curr.z <= epsilon || isect->t < p_curr.z) {
					tree++;
					level = tree & -tree;
					continue;
				}

				t = p_curr.z;
			}
			else {
				float l = len(p_en - p_st);
				/* minimum width extension */
				float or1 = r1;
				float or2 = r2;
				if(difl != 0.0f) {
					mw_extension = min(len(p_st - P) * difl, extmax);
					or1 = r1 < mw_extension ? mw_extension : r1;
					mw_extension = min(len(p_en - P) * difl, extmax);
					or2 = r2 < mw_extension ? mw_extension : r2;
				}
				/* --- */
				float3 tg = (p_en - p_st) / l;
				float gd = (or2 - or1) / l;
				float difz = -dot(p_st,tg);
				float cyla = 1.0f - (tg.z * tg.z * (1 + gd*gd));
				float halfb = (-p_st.z - tg.z*(difz + gd*(difz*gd + or1)));
				float tcentre = -halfb/cyla;
				float zcentre = difz + (tg.z * tcentre);
				float3 tdif = - p_st;
				tdif.z += tcentre;
				float tdifz = dot(tdif,tg);
				float tb = 2*(tdif.z - tg.z*(tdifz + gd*(tdifz*gd + or1)));
				float tc = dot(tdif,tdif) - tdifz * tdifz * (1 + gd*gd) - or1*or1 - 2*or1*tdifz*gd;
				float td = tb*tb - 4*cyla*tc;
				if (td < 0.0f) {
					tree++;
					level = tree & -tree;
					continue;
				}
				
				float rootd = sqrtf(td);
				float correction = ((-tb - rootd)/(2*cyla));
				t = tcentre + correction;

				float3 dp_st = (3 * curve_coef[3] * i_st + 2 * curve_coef[2]) * i_st + curve_coef[1];
				if (dot(tg, dp_st)< 0)
					dp_st *= -1;
				float3 dp_en = (3 * curve_coef[3] * i_en + 2 * curve_coef[2]) * i_en + curve_coef[1];
				if (dot(tg, dp_en) < 0)
					dp_en *= -1;

				if(flags & CURVE_KN_BACKFACING && (dot(dp_st, -p_st) + t * dp_st.z < 0 || dot(dp_en, p_en) - t * dp_en.z < 0 || isect->t < t || t <= 0.0f)) {
					correction = ((-tb + rootd)/(2*cyla));
					t = tcentre + correction;
				}			

				if (dot(dp_st, -p_st) + t * dp_st.z < 0 || dot(dp_en, p_en) - t * dp_en.z < 0 || isect->t < t || t <= 0.0f) {
					tree++;
					level = tree & -tree;
					continue;
				}

				float w = (zcentre + (tg.z * correction))/l;
				w = clamp((float)w, 0.0f, 1.0f);
				/* compute u on the curve segment */
				u = i_st * (1 - w) + i_en * w;
				r_curr = r1 + (r2 - r1) * w;
				r_ext = or1 + (or2 - or1) * w;
				coverage = r_curr/r_ext;

			}
			/* we found a new intersection */

			/* stochastic fade from minimum width */
			if(lcg_state && coverage != 1.0f) {
				if(lcg_step_float(lcg_state) > coverage)
					return hit;
			}

#ifdef __VISIBILITY_FLAG__
			/* visibility flag test. we do it here under the assumption
			 * that most triangles are culled by node flags */
			if(kernel_tex_fetch(__prim_visibility, curveAddr) & visibility)
#endif
			{
				/* record intersection */
				isect->prim = curveAddr;
				isect->object = object;
				isect->type = type;
				isect->u = u;
				isect->v = 0.0f;
				/*isect->v = 1.0f - coverage; */
				isect->t = t;
				hit = true;
			}
			
			tree++;
			level = tree & -tree;
		}
		else {
			/* split the curve into two curves and process */
			level = level >> 1;
		}
	}

	return hit;
}

ccl_device_inline bool bvh_curve_intersect(KernelGlobals *kg, Intersection *isect,
	float3 P, float3 idir, uint visibility, int object, int curveAddr, float time, int type, uint *lcg_state, float difl, float extmax)
{
	/* define few macros to minimize code duplication for SSE */
#ifndef __KERNEL_SSE2__
#define len3_squared(x) len_squared(x)
#define len3(x) len(x)
#define dot3(x, y) dot(x, y)
#endif

	int segment = PRIMITIVE_UNPACK_SEGMENT(type);
	/* curve Intersection check */
	int flags = kernel_data.curve.curveflags;

	int prim = kernel_tex_fetch(__prim_index, curveAddr);
	float4 v00 = kernel_tex_fetch(__curves, prim);

	int cnum = __float_as_int(v00.x);
	int k0 = cnum + segment;
	int k1 = k0 + 1;

#ifndef __KERNEL_SSE2__
	float4 P_curve[2];

	if(type & PRIMITIVE_CURVE) {
		P_curve[0]= kernel_tex_fetch(__curve_keys, k0);
		P_curve[1]= kernel_tex_fetch(__curve_keys, k1);
	}
	else {
		int fobject = (object == OBJECT_NONE)? kernel_tex_fetch(__prim_object, curveAddr): object;
		motion_curve_keys(kg, fobject, prim, time, k0, k1, P_curve);
	}

	float or1 = P_curve[0].w;
	float or2 = P_curve[1].w;
	float3 p1 = float4_to_float3(P_curve[0]);
	float3 p2 = float4_to_float3(P_curve[1]);

	/* minimum width extension */
	float r1 = or1;
	float r2 = or2;
	float3 dif = P - p1;
	float3 dif_second = P - p2;
	if(difl != 0.0f) {
		float pixelsize = min(len3(dif) * difl, extmax);
		r1 = or1 < pixelsize ? pixelsize : or1;
		pixelsize = min(len3(dif_second) * difl, extmax);
		r2 = or2 < pixelsize ? pixelsize : or2;
	}
	/* --- */

	float3 dir = 1.0f / idir;
	float3 p21_diff = p2 - p1;
	float3 sphere_dif1 = (dif + dif_second) * 0.5f;
	float sphere_b_tmp = dot3(dir, sphere_dif1);
	float3 sphere_dif2 = sphere_dif1 - sphere_b_tmp * dir;
#else
	__m128 P_curve[2];
	
	if(type & PRIMITIVE_CURVE) {
		P_curve[0] = _mm_load_ps(&kg->__curve_keys.data[k0].x);
		P_curve[1] = _mm_load_ps(&kg->__curve_keys.data[k1].x);
	}
	else {
		int fobject = (object == OBJECT_NONE)? kernel_tex_fetch(__prim_object, curveAddr): object;
		motion_curve_keys(kg, fobject, prim, time, k0, k1, (float4*)&P_curve);
	}

	const __m128 or12 = shuffle<3, 3, 3, 3>(P_curve[0], P_curve[1]);

	__m128 r12 = or12;
	const __m128 vP = load_m128(P);
	const __m128 dif = _mm_sub_ps(vP, P_curve[0]);
	const __m128 dif_second = _mm_sub_ps(vP, P_curve[1]);
	if(difl != 0.0f) {
		const __m128 len1_sq = len3_squared_splat(dif);
		const __m128 len2_sq = len3_squared_splat(dif_second);
		const __m128 len12 = _mm_sqrt_ps(shuffle<0, 0, 0, 0>(len1_sq, len2_sq));
		const __m128 pixelsize12 = _mm_min_ps(_mm_mul_ps(len12, _mm_set1_ps(difl)), _mm_set1_ps(extmax));
		r12 = _mm_max_ps(or12, pixelsize12);
	}
	float or1 = _mm_cvtss_f32(or12), or2 = _mm_cvtss_f32(broadcast<2>(or12));
	float r1 = _mm_cvtss_f32(r12), r2 = _mm_cvtss_f32(broadcast<2>(r12));

	const __m128 dir = _mm_div_ps(_mm_set1_ps(1.0f), load_m128(idir));
	const __m128 p21_diff = _mm_sub_ps(P_curve[1], P_curve[0]);
	const __m128 sphere_dif1 = _mm_mul_ps(_mm_add_ps(dif, dif_second), _mm_set1_ps(0.5f));
	const __m128 sphere_b_tmp = dot3_splat(dir, sphere_dif1);
	const __m128 sphere_dif2 = fnma(sphere_b_tmp, dir, sphere_dif1);
#endif

	float mr = max(r1, r2);
	float l = len3(p21_diff);
	float invl = 1.0f / l;
	float sp_r = mr + 0.5f * l;

	float sphere_b = dot3(dir, sphere_dif2);
	float sdisc = sphere_b * sphere_b - len3_squared(sphere_dif2) + sp_r * sp_r;

	if(sdisc < 0.0f)
		return false;

	/* obtain parameters and test midpoint distance for suitable modes */
#ifndef __KERNEL_SSE2__
	float3 tg = p21_diff * invl;
#else
	const __m128 tg = _mm_mul_ps(p21_diff, _mm_set1_ps(invl));
#endif
	float gd = (r2 - r1) * invl;

	float dirz = dot3(dir, tg);
	float difz = dot3(dif, tg);

	float a = 1.0f - (dirz*dirz*(1 + gd*gd));

	float halfb = dot3(dir, dif) - dirz*(difz + gd*(difz*gd + r1));

	float tcentre = -halfb/a;
	float zcentre = difz + (dirz * tcentre);

	if((tcentre > isect->t) && !(flags & CURVE_KN_ACCURATE))
		return false;
	if((zcentre < 0 || zcentre > l) && !(flags & CURVE_KN_ACCURATE) && !(flags & CURVE_KN_INTERSECTCORRECTION))
		return false;

	/* test minimum separation */
#ifndef __KERNEL_SSE2__
	float3 cprod = cross(tg, dir);
	float cprod2sq = len3_squared(cross(tg, dif));
#else
	const __m128 cprod = cross(tg, dir);
	float cprod2sq = len3_squared(cross_zxy(tg, dif));
#endif
	float cprodsq = len3_squared(cprod);
	float distscaled = dot3(cprod, dif);

	if(cprodsq == 0)
		distscaled = cprod2sq;
	else
		distscaled = (distscaled*distscaled)/cprodsq;

	if(distscaled > mr*mr)
		return false;

	/* calculate true intersection */
#ifndef __KERNEL_SSE2__
	float3 tdif = dif + tcentre * dir;
#else
	const __m128 tdif = fma(_mm_set1_ps(tcentre), dir, dif);
#endif
	float tdifz = dot3(tdif, tg);
	float tdifma = tdifz*gd + r1;
	float tb = 2*(dot3(dir, tdif) - dirz*(tdifz + gd*tdifma));
	float tc = dot3(tdif, tdif) - tdifz*tdifz - tdifma*tdifma;
	float td = tb*tb - 4*a*tc;

	if (td < 0.0f)
		return false;

	float rootd = 0.0f;
	float correction = 0.0f;
	if(flags & CURVE_KN_ACCURATE) {
		rootd = sqrtf(td);
		correction = ((-tb - rootd)/(2*a));
	}

	float t = tcentre + correction;

	if(t < isect->t) {

		if(flags & CURVE_KN_INTERSECTCORRECTION) {
			rootd = sqrtf(td);
			correction = ((-tb - rootd)/(2*a));
			t = tcentre + correction;
		}

		float z = zcentre + (dirz * correction);
		bool backface = false;

		if(flags & CURVE_KN_BACKFACING && (t < 0.0f || z < 0 || z > l)) {
			backface = true;
			correction = ((-tb + rootd)/(2*a));
			t = tcentre + correction;
			z = zcentre + (dirz * correction);
		}

		/* stochastic fade from minimum width */
		float adjradius = or1 + z * (or2 - or1) * invl;
		adjradius = adjradius / (r1 + z * gd);
		if(lcg_state && adjradius != 1.0f) {
			if(lcg_step_float(lcg_state) > adjradius)
				return false;
		}
		/* --- */

		if(t > 0.0f && t < isect->t && z >= 0 && z <= l) {

			if (flags & CURVE_KN_ENCLOSEFILTER) {
				float enc_ratio = 1.01f;
				if((difz > -r1 * enc_ratio) && (dot3(dif_second, tg) < r2 * enc_ratio)) {
					float a2 = 1.0f - (dirz*dirz*(1 + gd*gd*enc_ratio*enc_ratio));
					float c2 = dot3(dif, dif) - difz * difz * (1 + gd*gd*enc_ratio*enc_ratio) - r1*r1*enc_ratio*enc_ratio - 2*r1*difz*gd*enc_ratio;
					if(a2*c2 < 0.0f)
						return false;
				}
			}

#ifdef __VISIBILITY_FLAG__
			/* visibility flag test. we do it here under the assumption
			 * that most triangles are culled by node flags */
			if(kernel_tex_fetch(__prim_visibility, curveAddr) & visibility)
#endif
			{
				/* record intersection */
				isect->prim = curveAddr;
				isect->object = object;
				isect->type = type;
				isect->u = z*invl;
				isect->v = td/(4*a*a);
				/*isect->v = 1.0f - adjradius;*/
				isect->t = t;

				if(backface) 
					isect->u = -isect->u;
				
				return true;
			}
		}
	}

	return false;

#ifndef __KERNEL_SSE2__
#undef len3_squared
#undef len3
#undef dot3
#endif
}
#endif

#ifdef __HAIR__

ccl_device_inline float3 curvetangent(float t, float3 p0, float3 p1, float3 p2, float3 p3)
{
	float fc = 0.71f;
	float data[4];
	float t2 = t * t;
	data[0] = -3.0f * fc          * t2  + 4.0f * fc * t                  - fc;
	data[1] =  3.0f * (2.0f - fc) * t2  + 2.0f * (fc - 3.0f) * t;
	data[2] =  3.0f * (fc - 2.0f) * t2  + 2.0f * (3.0f - 2.0f * fc) * t  + fc;
	data[3] =  3.0f * fc          * t2  - 2.0f * fc * t;
	return data[0] * p0 + data[1] * p1 + data[2] * p2 + data[3] * p3;
}

ccl_device_inline float3 curvepoint(float t, float3 p0, float3 p1, float3 p2, float3 p3)
{
	float data[4];
	float fc = 0.71f;
	float t2 = t * t;
	float t3 = t2 * t;
	data[0] = -fc          * t3  + 2.0f * fc          * t2 - fc * t;
	data[1] =  (2.0f - fc) * t3  + (fc - 3.0f)        * t2 + 1.0f;
	data[2] =  (fc - 2.0f) * t3  + (3.0f - 2.0f * fc) * t2 + fc * t;
	data[3] =  fc          * t3  - fc * t2;
	return data[0] * p0 + data[1] * p1 + data[2] * p2 + data[3] * p3;
}

ccl_device_inline float3 bvh_curve_refine(KernelGlobals *kg, ShaderData *sd, const Intersection *isect, const Ray *ray)
{
	int flag = kernel_data.curve.curveflags;
	float t = isect->t;
	float3 P = ray->P;
	float3 D = ray->D;

	if(isect->object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_itfm;
#else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
		D = transform_direction(&tfm, D*t);
		D = normalize_len(D, &t);
	}

	int prim = kernel_tex_fetch(__prim_index, isect->prim);
	float4 v00 = kernel_tex_fetch(__curves, prim);

	int k0 = __float_as_int(v00.x) + PRIMITIVE_UNPACK_SEGMENT(sd->type);
	int k1 = k0 + 1;

	float3 tg;

	if(flag & CURVE_KN_INTERPOLATE) {
		int ka = max(k0 - 1,__float_as_int(v00.x));
		int kb = min(k1 + 1,__float_as_int(v00.x) + __float_as_int(v00.y) - 1);

		float4 P_curve[4];

		if(sd->type & PRIMITIVE_CURVE) {
			P_curve[0] = kernel_tex_fetch(__curve_keys, ka);
			P_curve[1] = kernel_tex_fetch(__curve_keys, k0);
			P_curve[2] = kernel_tex_fetch(__curve_keys, k1);
			P_curve[3] = kernel_tex_fetch(__curve_keys, kb);
		}
		else {
			motion_cardinal_curve_keys(kg, sd->object, sd->prim, sd->time, ka, k0, k1, kb, P_curve);
		}

		float l = 1.0f;
		tg = normalize_len(float4_to_float3(P_curve[2] - P_curve[1]), &l);
		float r1 = P_curve[1].w;
		float r2 = P_curve[2].w;
		float gd = ((r2 - r1)/l);
		
		P = P + D*t;

		float3 p[4];
		p[0] = float4_to_float3(P_curve[0]);
		p[1] = float4_to_float3(P_curve[1]);
		p[2] = float4_to_float3(P_curve[2]);
		p[3] = float4_to_float3(P_curve[3]);

#ifdef __UV__
		sd->u = isect->u;
		sd->v = 0.0f;
#endif
	
		tg = normalize(curvetangent(isect->u, p[0], p[1], p[2], p[3]));

		if(kernel_data.curve.curveflags & CURVE_KN_RIBBONS)
			sd->Ng = normalize(-(D - tg * (dot(tg, D))));
		else {
			float3 p_curr = curvepoint(isect->u, p[0], p[1], p[2], p[3]);	
			sd->Ng = normalize(P - p_curr);
			sd->Ng = sd->Ng - gd * tg;
			sd->Ng = normalize(sd->Ng);
		}
		sd->N = sd->Ng;
	}
	else {
		float4 P_curve[2];

		if(sd->type & PRIMITIVE_CURVE) {
			P_curve[0]= kernel_tex_fetch(__curve_keys, k0);
			P_curve[1]= kernel_tex_fetch(__curve_keys, k1);
		}
		else {
			motion_curve_keys(kg, sd->object, sd->prim, sd->time, k0, k1, P_curve);
		}

		float l = 1.0f;
		tg = normalize_len(float4_to_float3(P_curve[1] - P_curve[0]), &l);
		float r1 = P_curve[0].w;
		float r2 = P_curve[1].w;
		float gd = ((r2 - r1)/l);
		
		P = P + D*t;

		float3 dif = P - float4_to_float3(P_curve[0]);

#ifdef __UV__
		sd->u = dot(dif,tg)/l;
		sd->v = 0.0f;
#endif

		if (flag & CURVE_KN_TRUETANGENTGNORMAL) {
			sd->Ng = -(D - tg * dot(tg, D));
			sd->Ng = normalize(sd->Ng);
		}
		else {
			sd->Ng = (dif - tg * sd->u * l) / (P_curve[0].w + sd->u * l * gd);
			if (gd != 0.0f) {
				sd->Ng = sd->Ng - gd * tg ;
				sd->Ng = normalize(sd->Ng);
			}
		}

		sd->N = sd->Ng;
	}

#ifdef __DPDU__
	/* dPdu/dPdv */
	sd->dPdu = tg;
	sd->dPdv = cross(tg, sd->Ng);
#endif

	/*add fading parameter for minimum pixel width with transparency bsdf*/
	/*sd->curve_transparency = isect->v;*/
	/*sd->curve_radius = sd->u * gd * l + r1;*/

	if(isect->object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_tfm;
#else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
	}

	return P;
}
#endif

CCL_NAMESPACE_END
