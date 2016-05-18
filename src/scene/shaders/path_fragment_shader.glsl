/* *********************************************************************************************** */

// #shader_type fragment

// error: cannot initialize uniform cap_type in GLSL 1.10 (GLSL 1.20 required)
#version 120

/* *********************************************************************************************** */

uniform lowp float qt_Opacity;

uniform vec4 colour = vec4(1);
uniform int cap_type = 1;
uniform int line_join = 1;
uniform float antialias_diameter = 1;

/* *********************************************************************************************** */

varying highp vec2 uv;
varying float line_length;
varying float line_width;
/* varying vec4 colour; */
varying float cap;

/* *********************************************************************************************** */

// Compute distance to cap
float
distance_to_cap(int type, float dx, float dy, float t)
{
  return length(vec2(dx, dy));
}

// Compute distance to join
float
join(in vec2 uv, in float line_stop, in float line_width)
{
  float u = uv.x;
  float d = abs(uv.y);

  if (u < 0)
    d = max(d, length(uv));
  else if (u > line_stop)
    d = max(d, length(uv - vec2(line_stop, 0)));

  return d;
}

/* *********************************************************************************************** */

void main()
{
  // If colour is fully transparent we just discard the fragment
  if (colour.a <= 0)
    discard;

  vec4 frag_color;

  float u = uv.x;
  float v = uv.y;
  float t = line_width/2 - antialias_diameter;

  float dy = abs(v);

  float line_start = 0;
  float line_stop = line_length;

  float d = 0;
  // start cap
  if (cap == -1) { //  && u < .0
    d = distance_to_cap(cap_type, abs(u), dy, t);
  }
  // stop cap
  else if (cap == 1 && u > line_stop)
    d = distance_to_cap(cap_type, abs(u) - line_stop, dy, t);
  else
    d = join(uv, line_stop, line_width);

  // Anti-alias test, distance to border
  d -= t;
  if (d < 0) {
    frag_color = colour;
    /* frag_color = vec4(0, 1, 1, 1); */
  } else {
    d /= antialias_diameter;
    frag_color = vec4(colour.xyz, exp(-d*d) * colour.a);
    /* frag_color = vec4(1, v, u, 1); */
  }

  // frag_color = vec4(0, 0, 0, 1);
  gl_FragColor = frag_color * qt_Opacity;
}

/* *********************************************************************************************** *
 *
 * End
 *
 * *********************************************************************************************** */