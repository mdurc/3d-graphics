#ifndef _LIB_MATH_H
#define _LIB_MATH_H

#include <math.h>

#include "macros.h"
#include "types.h"

#define min(_a, _b) ((_a) < (_b) ? (_a) : (_b))
#define max(_a, _b) ((_a) > (_b) ? (_a) : (_b))
#define clamp(_x, _mi, _ma) (max(_mi, min(_x, _ma)))

// iv2 and if2 specific rudimentary macros
#define dot(v0, v1) ({ const fv2 _v0 = (v0), _v1 = (v1); (_v0.x * _v1.x) + (_v0.y * _v1.y); })
#define length(v) ({ const fv2 _v = (v); sqrtf(dot(_v, _v)); })
#define normalize(u) ({ const fv2 _u = (u); const f32 _l = length(_u); (fv2) { _u.x / _l, _u.y / _l }; })
#define sign(_a) ({ __typeof__(_a) __a = (_a); (__typeof__(_a))(__a < 0 ? -1 : (__a > 0 ? 1 : 0)); })

#define FLOAT_EPSILON 1e-10f
#define float_eq(a, b) (fabsf((a) - (b)) < FLOAT_EPSILON)

// vector and matrix utilities
#define MATH_H_DEFINE_VEC(n) \
typedef f32 vec##n[n]; \
\
M_INLINE void vec##n##_add(vec##n r, vec##n const a, vec##n const b) { \
    for (u32 i = 0; i < n; ++i) r[i] = a[i] + b[i]; \
} \
\
M_INLINE void vec##n##_sub(vec##n r, vec##n const a, vec##n const b) { \
    for (u32 i = 0; i < n; ++i) r[i] = a[i] - b[i]; \
} \
\
M_INLINE void vec##n##_scale(vec##n r, vec##n const v, f32 const s) { \
    for (u32 i = 0; i < n; ++i) r[i] = v[i] * s; \
} \
\
M_INLINE f32 vec##n##_dot(vec##n const a, vec##n const b) { \
    f32 p = 0.0f; \
    for (u32 i = 0; i < n; ++i) p += b[i] * a[i]; \
    return p; \
} \
\
M_INLINE f32 vec##n##_len(vec##n const v) { \
    return sqrtf(vec##n##_dot(v, v)); \
} \
\
M_INLINE void vec##n##_normalize(vec##n r, vec##n const v) { \
    f32 l = vec##n##_len(v); \
    if (l > FLOAT_EPSILON) { \
        f32 k = 1.0f / l; \
        vec##n##_scale(r, v, k); \
    } else { \
        for (u32 i = 0; i < n; ++i) r[i] = 0.0f; \
    } \
} \
\
M_INLINE void vec##n##_dup(vec##n r, vec##n const src) { \
    for (u32 i = 0; i < n; ++i) r[i] = src[i]; \
}

MATH_H_DEFINE_VEC(2)
MATH_H_DEFINE_VEC(3)
MATH_H_DEFINE_VEC(4)

M_INLINE void vec3_cross(vec3 r, vec3 const a, vec3 const b) {
  r[0] = a[1] * b[2] - a[2] * b[1];
  r[1] = a[2] * b[0] - a[0] * b[2];
  r[2] = a[0] * b[1] - a[1] * b[0];
}

M_INLINE void vec4_cross(vec4 r, vec4 const a, vec4 const b) {
  r[0] = a[1] * b[2] - a[2] * b[1];
  r[1] = a[2] * b[0] - a[0] * b[2];
  r[2] = a[0] * b[1] - a[1] * b[0];
  r[3] = 1.0f;
}

// Note: row-major format, thus, must use GL_TRUE in glUniformMatrix transpose
// mat[row][column]
// This is not the standard for OpenGL.

typedef vec4 mat4x4[4];

M_INLINE void mat4x4_mov(mat4x4 dest, mat4x4 const src) {
  memcpy(dest, src, sizeof(mat4x4));
}

M_INLINE void mat4x4_identity(mat4x4 M) {
  memset(M, 0, sizeof(mat4x4));
  M[0][0] = 1.0f;
  M[1][1] = 1.0f;
  M[2][2] = 1.0f;
  M[3][3] = 1.0f;
}

M_INLINE void mat4x4_mul(mat4x4 M, mat4x4 const a, mat4x4 const b) {
  mat4x4 temp;
  for (u32 r = 0; r < 4; ++r) {
    for (u32 c = 0; c < 4; ++c) {
      temp[r][c] = 0.0f;
      for (u32 i = 0; i < 4; ++i) {
        temp[r][c] += a[r][i] * b[i][c];
      }
    }
  }
  mat4x4_mov(M, temp);
}

M_INLINE void mat4x4_from_translation(mat4x4 T, f32 x, f32 y, f32 z) {
  /*
     1.0f, 0.0f, 0.0f, x, // x translation
     0.0f, 1.0f, 0.0f, y, // y translation
     0.0f, 0.0f, 1.0f, z, // z translation
     0.0f, 0.0f, 0.0f, 1.0f,
  */
  mat4x4_identity(T);
  T[0][3] = x;
  T[1][3] = y;
  T[2][3] = z;
}

M_INLINE void mat4x4_translate(mat4x4 M, f32 x, f32 y, f32 z) {
  mat4x4 t;
  mat4x4_from_translation(t, x, y, z);
  mat4x4_mul(M, M, t);
}

/*
   Note that the outermost row/col is for the translation, so we really only
   need a 3x3 for 3d rotation, though the extra trick is useful.

   Solving for the 2d rotation matrix:
   in polar, we know that x = r cos(t) and y = r sin(t)
   thus, for some arbitrary rotation p: x' = r cos(t + p), y' = r sin(t + p)

   Using trig identities, we can rewrite x' and y' as:

   x' = r(cos(p)cos(t)) - r(sin(p)sin(t))
   y' = r(sin(p)cos(t)) + r(cos(p)sin(t))

   thus, substituting in x or y for their respective polar coordinate values:

   x' = xcos(p) - ysin(p)
   y' = xsin(p) + ycos(p)

   thus, we know that the matrix to multiply by the position vector is:
   |cos -sin| times |x|
   |sin  cos|       |y|

   For 3d, to rotate around the z-axis, it is the same as this 2d rotation,
   and just have to make it so that z is not affected.
*/
M_INLINE void mat4x4_rotate_z(mat4x4 result, mat4x4 const M, f32 theta) {
  f32 s = sinf(theta);
  f32 c = cosf(theta);
  mat4x4 R = {
      {c, -s, 0.0f, 0.0f},
      {s, c, 0.0f, 0.0f},
      {0.0f, 0.0f, 1.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 1.0f},
  };
  mat4x4_mul(result, M, R);
}

/*
   For rotation around the x and y axis, we can imagine a simple rotation of the
   entire coordinate plane and what the corresponding values for 'x' and 'y'
   would be in the formula for the 2d rotation matrix.

   For example, in the rotation around the x-axis:
   imagine that x is the vertical (old z) axis. If this were to happen, then
   the y axis must have been rotated to become the old x-axis, and the z axis
   would have been rotated to become the old y-axis.

   after rotating the entire 3d coordinate plane, we can note that now:
   z = rsin(t) and y = rcos(t) and following the formula from before:
   y' = cos(p)y - sin(p)z
   z' = sin(p)y + cos(p)z
*/
M_INLINE void mat4x4_rotate_x(mat4x4 result, mat4x4 const M, f32 theta) {
  f32 s = sinf(theta);
  f32 c = cosf(theta);
  mat4x4 R = {
      {1.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, c, -s, 0.0f},
      {0.0f, s, c, 0.0f},
      {0.0f, 0.0f, 0.0f, 1.0f},
  };
  mat4x4_mul(result, M, R);
}

/*
   Same as with the x-axis rotation, we can imagine that the entire 3d
   coordinate plane is rotated to have the y-axis pointing upwards. Then the new
   polar coordinates would be: x = rsin(t) and z = rcos(t).
   Thus:
   x = sin(p)z + cos(p)x
   z = cos(p)z - sin(p)x
*/
M_INLINE void mat4x4_rotate_y(mat4x4 result, mat4x4 const M, f32 theta) {
  f32 s = sinf(theta);
  f32 c = cosf(theta);
  mat4x4 R = {
      {c, 0.0f, s, 0.0f},
      {0.0f, 1.0f, 0.0f, 0.0f},
      {-s, 0.0f, c, 0.0f},
      {0.0f, 0.0f, 0.0f, 1.0f},
  };
  mat4x4_mul(result, M, R);
}

/*
   Opengl will automatically divide the homogeneous component W on the gpu.
   Thus we can set W to Z to scale Z for depth.
   Note that in opengl:
   - Right-handed view space: camera is at (0,0,0) looking down the -Z axis
   - Left-handed NDC space: [-1, 1] cube where the +Z is behind the cube
   It is possible to make it so that the NDC space is also right handed,
   though that is not the opengl convention/standard. Because of this, we can
   set W to be negative for proper scaling.

   | c 0 0  0 | times |x| equals |xc|
   | 0 d 0  0 |       |y|        |yd|
   | 0 0 a  b |       |z|        |za + b|
   | 0 0 -1 0 |       |1|        |-z|

   We can then solve for za + b to make sure that it is bound in the NDC range
   of [-1, 1]. We can then plug in the two extreme distances in for z, note
   that they will be -n and -f, and negative because they are not behind the
   screen (LHS opengl standard). Thus we have two constraints to solve a
   system of equations:

   z_ndc = z_clip / w_clip = (az + b) / -(z)

   so:
   (a(-n) + b) / -(-n) = -1
   (a(-f) + b) / -(-f) =  1

   b = -n + an
   -af - n + an = f
   a(-f + n) = f + n

   a = (f + n) / (n - f)
   b = -n + (fn + n^2) / (n - f) = 2fn / (n - f)

   For a simple orthographic projection, just leave c and d as 1.0f.
   We already have scaling now proportional to the Z component for depth. We
   can further customize our projection based on FOV and aspect ratio (y and x
   scaling). Just like with z:
   x_ndc = x_clip / w_clip = (xc) / -(z)
   y_ndc = y_clip / w_clip = (yd) / -(z)

   If you image a side-view of the camera to the near facing plane on the
   screen, the cone can be split down the middle to create two
   right-triangles. The top right vertex is y_top, and the bottom right vertex
   is y_bot, and the FOV is the full angle theta. We know that the height:
   y_top = n * tan(FOV/2).
   - We can generalize this by noting that this relationship is constant
   between any top edge, thus y = -z * tan(FOV/2).

   To solve for d, the y-scaling, plug it into y_ndc:
   y_ndc = (-z * tan(FOV/2)) d / -z = tan(FOV/2) * d,
   thus d = y_ndc / tan(FOV/2),
   thus, since we want to map y_ndc to 1 at this top edge, let y_ndc = 1:
   then, d = 1 / tan(FOV/2)

   To solve for c, the x-scaling, use the aspect ratio (width / height) to
   counteract any stretching: c = d / aspect_ratio
*/
M_INLINE void mat4x4_perspective(mat4x4 M, f32 y_fov_radians, f32 aspect, f32 n,
                                 f32 f) {
  // y_fov is in radians
  f32 d = 1.0f / tanf(y_fov_radians / 2.0f);
  f32 c = d / aspect;
  f32 a = (f + n) / (n - f);
  f32 b = (2.0f * f * n) / (n - f);

  mat4x4_identity(M);
  M[0][0] = c;
  M[1][1] = d;
  M[2][2] = a;
  M[2][3] = b;
  M[3][2] = -1.0f;
  M[3][3] = 0.0f;
}

M_INLINE void mat4x4_scale_aniso(mat4x4 result, mat4x4 const M, f32 sx, f32 sy,
                                 f32 sz) {
  mat4x4 S;
  mat4x4_identity(S);
  S[0][0] = sx;
  S[1][1] = sy;
  S[2][2] = sz;
  mat4x4_mul(result, M, S);
}

M_INLINE void mat4x4_ortho(mat4x4 M, f32 l, f32 r, f32 b, f32 t, f32 n, f32 f) {
  mat4x4_identity(M);
  M[0][0] = 2.0f / (r - l);
  M[1][1] = 2.0f / (t - b);
  M[2][2] = -2.0f / (f - n);
  M[0][3] = (r + l) / (l - r);
  M[1][3] = (t + b) / (b - t);
  M[2][3] = (f + n) / (n - f);
}

M_INLINE void mat4x4_look_at(mat4x4 M, vec3 const eye, vec3 const center,
                             vec3 const up) {
  vec3 f, s, t;

  // z-axis
  vec3_sub(f, center, eye);
  vec3_normalize(f, f);

  // x-axis
  vec3_cross(s, f, up);
  vec3_normalize(s, s);

  // y-axis
  vec3_cross(t, s, f);

  M[0][0] = s[0];
  M[0][1] = s[1];
  M[0][2] = s[2];
  M[0][3] = -vec3_dot(s, eye);

  M[1][0] = t[0];
  M[1][1] = t[1];
  M[1][2] = t[2];
  M[1][3] = -vec3_dot(t, eye);

  M[2][0] = -f[0];
  M[2][1] = -f[1];
  M[2][2] = -f[2];
  M[2][3] = vec3_dot(f, eye);

  M[3][0] = 0.0f;
  M[3][1] = 0.0f;
  M[3][2] = 0.0f;
  M[3][3] = 1.0f;
}

M_INLINE void mat4x4_transpose(mat4x4 M, mat4x4 const src) {
  mat4x4 res;
  for (u32 r = 0; r < 4; ++r) {
    for (u32 c = 0; c < 4; ++c) {
      res[r][c] = src[c][r];
    }
  }
  mat4x4_mov(M, res);
}

M_INLINE bool mat4x4_invert(mat4x4 M, mat4x4 const src) {
  mat4x4 inv;
  f32 det;

  inv[0][0] =
      src[1][1] * src[2][2] * src[3][3] - src[1][1] * src[2][3] * src[3][2] -
      src[2][1] * src[1][2] * src[3][3] + src[2][1] * src[1][3] * src[3][2] +
      src[3][1] * src[1][2] * src[2][3] - src[3][1] * src[1][3] * src[2][2];

  inv[0][1] =
      -src[0][1] * src[2][2] * src[3][3] + src[0][1] * src[2][3] * src[3][2] +
      src[2][1] * src[0][2] * src[3][3] - src[2][1] * src[0][3] * src[3][2] -
      src[3][1] * src[0][2] * src[2][3] + src[3][1] * src[0][3] * src[2][2];

  inv[0][2] =
      src[0][1] * src[1][2] * src[3][3] - src[0][1] * src[1][3] * src[3][2] -
      src[1][1] * src[0][2] * src[3][3] + src[1][1] * src[0][3] * src[3][2] +
      src[3][1] * src[0][2] * src[1][3] - src[3][1] * src[0][3] * src[1][2];

  inv[0][3] =
      -src[0][1] * src[1][2] * src[2][3] + src[0][1] * src[1][3] * src[2][2] +
      src[1][1] * src[0][2] * src[2][3] - src[1][1] * src[0][3] * src[2][2] -
      src[2][1] * src[0][2] * src[1][3] + src[2][1] * src[0][3] * src[1][2];

  inv[1][0] =
      -src[1][0] * src[2][2] * src[3][3] + src[1][0] * src[2][3] * src[3][2] +
      src[2][0] * src[1][2] * src[3][3] - src[2][0] * src[1][3] * src[3][2] -
      src[3][0] * src[1][2] * src[2][3] + src[3][0] * src[1][3] * src[2][2];

  inv[1][1] =
      src[0][0] * src[2][2] * src[3][3] - src[0][0] * src[2][3] * src[3][2] -
      src[2][0] * src[0][2] * src[3][3] + src[2][0] * src[0][3] * src[3][2] +
      src[3][0] * src[0][2] * src[2][3] - src[3][0] * src[0][3] * src[2][2];

  inv[1][2] =
      -src[0][0] * src[1][2] * src[3][3] + src[0][0] * src[1][3] * src[3][2] +
      src[1][0] * src[0][2] * src[3][3] - src[1][0] * src[0][3] * src[3][2] -
      src[3][0] * src[0][2] * src[1][3] + src[3][0] * src[0][3] * src[1][2];

  inv[1][3] =
      src[0][0] * src[1][2] * src[2][3] - src[0][0] * src[1][3] * src[2][2] -
      src[1][0] * src[0][2] * src[2][3] + src[1][0] * src[0][3] * src[2][2] +
      src[2][0] * src[0][2] * src[1][3] - src[2][0] * src[0][3] * src[1][2];

  inv[2][0] =
      src[1][0] * src[2][1] * src[3][3] - src[1][0] * src[2][3] * src[3][1] -
      src[2][0] * src[1][1] * src[3][3] + src[2][0] * src[1][3] * src[3][1] +
      src[3][0] * src[1][1] * src[2][3] - src[3][0] * src[1][3] * src[2][1];

  inv[2][1] =
      -src[0][0] * src[2][1] * src[3][3] + src[0][0] * src[2][3] * src[3][1] +
      src[2][0] * src[0][1] * src[3][3] - src[2][0] * src[0][3] * src[3][1] -
      src[3][0] * src[0][1] * src[2][3] + src[3][0] * src[0][3] * src[2][1];

  inv[2][2] =
      src[0][0] * src[1][1] * src[3][3] - src[0][0] * src[1][3] * src[3][1] -
      src[1][0] * src[0][1] * src[3][3] + src[1][0] * src[0][3] * src[3][1] +
      src[3][0] * src[0][1] * src[1][3] - src[3][0] * src[0][3] * src[1][1];

  inv[2][3] =
      -src[0][0] * src[1][1] * src[2][3] + src[0][0] * src[1][3] * src[2][1] +
      src[1][0] * src[0][1] * src[2][3] - src[1][0] * src[0][3] * src[2][1] -
      src[2][0] * src[0][1] * src[1][3] + src[2][0] * src[0][3] * src[1][1];

  inv[3][0] =
      -src[1][0] * src[2][1] * src[3][2] + src[1][0] * src[2][2] * src[3][1] +
      src[2][0] * src[1][1] * src[3][2] - src[2][0] * src[1][2] * src[3][1] -
      src[3][0] * src[1][1] * src[2][2] + src[3][0] * src[1][2] * src[2][1];

  inv[3][1] =
      src[0][0] * src[2][1] * src[3][2] - src[0][0] * src[2][2] * src[3][1] -
      src[2][0] * src[0][1] * src[3][2] + src[2][0] * src[0][2] * src[3][1] +
      src[3][0] * src[0][1] * src[2][2] - src[3][0] * src[0][2] * src[2][1];

  inv[3][2] =
      -src[0][0] * src[1][1] * src[3][2] + src[0][0] * src[1][2] * src[3][1] +
      src[1][0] * src[0][1] * src[3][2] - src[1][0] * src[0][2] * src[3][1] -
      src[3][0] * src[0][1] * src[1][2] + src[3][0] * src[0][2] * src[1][1];

  inv[3][3] =
      src[0][0] * src[1][1] * src[2][2] - src[0][0] * src[1][2] * src[2][1] -
      src[1][0] * src[0][1] * src[2][2] + src[1][0] * src[0][2] * src[2][1] +
      src[2][0] * src[0][1] * src[1][2] - src[2][0] * src[0][2] * src[1][1];

  det = src[0][0] * inv[0][0] + src[0][1] * inv[1][0] + src[0][2] * inv[2][0] +
        src[0][3] * inv[3][0];

  // check if invertible
  if (fabsf(det) < FLOAT_EPSILON) return false;

  det = 1.0f / det;

  for (u32 r = 0; r < 4; ++r) {
    for (u32 c = 0; c < 4; ++c) {
      inv[r][c] *= det;
    }
  }

  mat4x4_mov(M, inv);
  return true;
}

#endif
