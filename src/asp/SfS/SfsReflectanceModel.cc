// __BEGIN_LICENSE__
//  Copyright (c) 2009-2013, United States Government as represented by the
//  Administrator of the National Aeronautics and Space Administration. All
//  rights reserved.
//
//  The NGT platform is licensed under the Apache License, Version 2.0 (the
//  "License"); you may not use this file except in compliance with the
//  License. You may obtain a copy of the License at
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
// __END_LICENSE__

// \file SfsReflectanceModel.cc
// The reflectance models used by SfS.

#include <asp/SfS/SfsReflectanceModel.h>
#include <vw/Cartography/GeoReference.h>

namespace asp {

using namespace vw;

// Computes the Lambertian reflectance model (cosine of the light
// direction and the normal to the Moon) Vector3 sunpos: the 3D
// coordinates of the Sun relative to the center of the Moon Vector2
// lon_lat is a 2D vector. First element is the longitude and the
// second the latitude.
// Author: Ara Nefian
double
LambertianReflectance(Vector3 sunPos, Vector3 xyz, Vector3 normal) {
  double reflectance;
  Vector3 sunDirection = normalize(sunPos-xyz);

  reflectance
   = sunDirection[0]*normal[0] + sunDirection[1]*normal[1] + sunDirection[2]*normal[2];

  return reflectance;
}

double LunarLambertianReflectance(Vector3 const& sunPos,
                                  Vector3 const& viewPos,
                                  Vector3 const& xyz,
                                  Vector3 const& normal,
                                  double phaseCoeffC1,
                                  double phaseCoeffC2,
                                  double & alpha,
                                  const double * refl_coeffs) {
  double reflectance;
  double L;

  double len = dot_prod(normal, normal);
  if (abs(len - 1.0) > 1.0e-4)
    vw::vw_throw(vw::ArgumentErr() 
                 << "Expecting unit normal in the reflectance computation.\n");

  // Compute mu_0 = cosine of the angle between the light direction and the surface normal.
  //sun coordinates relative to the xyz point on the Moon surface
  Vector3 sunDirection = normalize(sunPos-xyz);
  double mu_0 = dot_prod(sunDirection, normal);

  // Compute mu = cosine of the angle between the viewer direction and the surface normal.
  // viewer coordinates relative to the xyz point on the Moon surface
  Vector3 viewDirection = normalize(viewPos-xyz);
  double mu = dot_prod(viewDirection,normal);

  //compute the phase angle (alpha) between the viewing direction and the light source direction
  double deg_alpha;
  double cos_alpha;

  double tol = 1e-8;
  cos_alpha = dot_prod(sunDirection, viewDirection);
  if ((cos_alpha > 1 + tol) || (cos_alpha < -1 - tol))
    vw::vw_throw(vw::ArgumentErr() << "cos(alpha) error.\n");

  alpha     = acos(cos_alpha);  // phase angle in radians
  deg_alpha = alpha*180.0/M_PI; // phase angle in degrees

  // Bob Gaskell's model
  // L = exp(-deg_alpha/60.0);

  // Alfred McEwen's model
  double O = refl_coeffs[0]; // 1
  double A = refl_coeffs[1]; //-0.019;
  double B = refl_coeffs[2]; // 0.000242;//0.242*1e-3;
  double C = refl_coeffs[3]; // -0.00000146;//-1.46*1e-6;

  L = O + A*deg_alpha + B*deg_alpha*deg_alpha + C*deg_alpha*deg_alpha*deg_alpha;
 
  reflectance = 2*L*mu_0/(mu_0+mu) + (1-L)*mu_0;
  if (mu_0 + mu == 0 || reflectance != reflectance)
    return 0.0;

  // Attempt to compensate for points on the terrain being too bright
  // if the sun is behind the spacecraft as seen from those points.
  reflectance *= (exp(-phaseCoeffC1*alpha) + phaseCoeffC2);

  return reflectance;
}

// Hapke's model. See: An Experimental Study of Light Scattering by Large,
// Irregular Particles Audrey F. McGuire, Bruce W. Hapke. 1995. The reflectance
// used is R(g), in equation above Equation 21. The p(g) function is given by
// Equation (14), yet this one uses an old convention. The updated p(g) is given
// in: Spectrophotometric properties of materials observed by Pancam on the Mars
// Exploration Rovers: 1. Spirit. JR Johnson, 2006. We Use the two-term p(g),
// and the parameter c, not c'=1-c. We also use the values of w(=omega), b, and
// c from that table.

// Note that we use the updated Hapke model, having the term B(g). This one is
// given in "Modeling spectral and bidirectional reflectance", Jacquemoud, 1992.
// It has the params B0 and h. The ultimate reference is probably Hapke, 1986,
// having all pieces in one place, but that one is not available. 

// We use mostly the parameter values for omega, b, c, B0 and h from: Surface
// reflectance of Mars observed by CRISM/MRO: 2. Estimation of surface
// photometric properties in Gusev Crater and Meridiani Planum by J. Fernando.
// See equations (1), (2) and (4) in that paper.

// Example values for the params: w=omega=0.68, b=0.17, c=0.62, B0=0.52, h=0.52.

// We don't use equation (3) from that paper, we use instead what they call
// the formula H93, which is the H(x) from McGuire and Hapke 1995 mentioned
// above. See the complete formulas below.

// The Fernando paper has a factor S, which is not present in the 1992
// Jacquemoud paper, so we don't use it either here.
double HapkeReflectance(Vector3 const& sunPos,
                        Vector3 const& viewPos,
                        Vector3 const& xyz,
                        Vector3 const& normal,
                        double phaseCoeffC1,
                        double phaseCoeffC2,
                        double & alpha,
                        const double * refl_coeffs) {

  double len = dot_prod(normal, normal);
  if (abs(len - 1.0) > 1.0e-4)
    vw::vw_throw(vw::ArgumentErr() 
                 << "Expecting unit normal in the reflectance computation.\n");

  //compute mu_0 = cosine of the angle between the light direction and the surface normal.
  //sun coordinates relative to the xyz point on the Moon surface
  Vector3 sunDirection = normalize(sunPos-xyz);
  double mu_0 = dot_prod(sunDirection, normal);

  //compute mu = cosine of the angle between the viewer direction and the surface normal.
  //viewer coordinates relative to the xyz point on the Moon surface
  Vector3 viewDirection = normalize(viewPos-xyz);
  double mu = dot_prod(viewDirection,normal);

  //compute the phase angle (g) between the viewing direction and the light source direction
  // in radians
  double cos_g = dot_prod(sunDirection, viewDirection);
  double g = acos(cos_g);  // phase angle in radians

  // Hapke params
  double omega = std::abs(refl_coeffs[0]); // also known as w
  double b     = std::abs(refl_coeffs[1]);
  double c     = std::abs(refl_coeffs[2]);
  // The older Hapke model lacks the B0 and h terms
  double B0    = std::abs(refl_coeffs[3]);
  double h     = std::abs(refl_coeffs[4]);   

  // Does not matter, we'll factor out the constant scale as camera exposures anyway
  double J = 1.0; 
  
  // The P(g) term
  double Pg 
    = (1.0 - c) * (1.0 - b*b) / pow(1.0 + 2.0*b*cos_g + b*b, 1.5)
    + c         * (1.0 - b*b) / pow(1.0 - 2.0*b*cos_g + b*b, 1.5);
    
  // The B(g) term
  double Bg = B0 / (1.0 + (1.0/h)*tan(g/2.0));

  double H_mu0 = (1.0 + 2*mu_0) / (1.0 + 2*mu_0 * sqrt(1.0 - omega));
  double H_mu  = (1.0 + 2*mu  ) / (1.0 + 2*mu   * sqrt(1.0 - omega));

  // The reflectance
  double R = (J*omega/4.0/M_PI) * (mu_0/(mu_0+mu)) * ((1.0 + Bg)*Pg + H_mu0*H_mu - 1.0);
  
  return R;
}

// Use the following model:
// Reflectance = f(alpha) * A * mu_0 /(mu_0 + mu) + (1-A) * mu_0
// The value of A is either 1 (the so-called lunar-model), or A=0.7.
// f(alpha) = 0.63.
double CharonReflectance(Vector3 const& sunPos,
                         Vector3 const& viewPos,
                         Vector3 const& xyz,
                         Vector3 const& normal,
                         double phaseCoeffC1,
                         double phaseCoeffC2,
                         double & alpha,
                         const double * refl_coeffs) {

  double len = dot_prod(normal, normal);
  if (abs(len - 1.0) > 1.0e-4)
    vw::vw_throw(vw::ArgumentErr() 
                 << "Expecting unit normal in the reflectance computation.\n");

  //compute mu_0 = cosine of the angle between the light direction and the surface normal.
  //sun coordinates relative to the xyz point on the Moon surface
  Vector3 sunDirection = normalize(sunPos-xyz);
  double mu_0 = dot_prod(sunDirection, normal);

  //compute mu = cosine of the angle between the viewer direction and the surface normal.
  //viewer coordinates relative to the xyz point on the Moon surface
  Vector3 viewDirection = normalize(viewPos-xyz);
  double mu = dot_prod(viewDirection,normal);

  // Charon model params
  double A       = std::abs(refl_coeffs[0]); // albedo 
  double f_alpha = std::abs(refl_coeffs[1]); // phase function 

  double reflectance = f_alpha*A*mu_0 / (mu_0 + mu) + (1.0 - A)*mu_0;
  
  if (mu_0 + mu == 0 || reflectance != reflectance){
    return 0.0;
  }

  return reflectance;
}

// Lunar-Lambertian with duplicated coefficients that could be optimized separately.
// This is experimental.
double ExperimentalLunarLambertianReflectance(Vector3 const& sunPos,
                                              Vector3 const& viewPos,
                                              Vector3 const& xyz,
                                              Vector3 const& normal,
                                              double phaseCoeffC1,
                                              double phaseCoeffC2,
                                              double & alpha,
                                              const double * refl_coeffs) {
  double reflectance;

  double len = dot_prod(normal, normal);
  if (std::abs(len - 1.0) > 1.0e-4)
    vw::vw_throw(vw::ArgumentErr() 
                 << "Expecting unit normal in the reflectance computation.\n");

  // Compute mu_0 = cosine of the angle between the light direction and the surface normal.
  // sun coordinates relative to the xyz point on the Moon surface
  // Vector3 sunDirection = -normalize(sunPos-xyz);
  Vector3 sunDirection = normalize(sunPos-xyz);
  double mu_0 = dot_prod(sunDirection, normal);

  //compute mu = cosine of the angle between the viewer direction and the surface normal.
  //viewer coordinates relative to the xyz point on the Moon surface
  Vector3 viewDirection = normalize(viewPos-xyz);
  double mu = dot_prod(viewDirection,normal);

  //compute the phase angle (alpha) between the viewing direction and the light source direction
  double deg_alpha;
  double cos_alpha;

  double tol = 1e-8;
  cos_alpha = dot_prod(sunDirection,viewDirection);
  if (cos_alpha > 1 + tol || cos_alpha < -1 - tol)
    vw::vw_throw(vw::ArgumentErr() << "cos_alpha error\n");

  alpha     = acos(cos_alpha);  // phase angle in radians
  deg_alpha = alpha*180.0/M_PI; // phase angle in degrees

  //Alfred McEwen's model
  double O1 = refl_coeffs[0]; // 1
  double A1 = refl_coeffs[1]; // -0.019;
  double B1 = refl_coeffs[2]; // 0.000242;//0.242*1e-3;
  double C1 = refl_coeffs[3]; // -0.00000146;//-1.46*1e-6;
  double D1 = refl_coeffs[4]; 
  double E1 = refl_coeffs[5]; 
  double F1 = refl_coeffs[6]; 
  double G1 = refl_coeffs[7]; 

  double O2 = refl_coeffs[8];  // 1
  double A2 = refl_coeffs[9];  // -0.019;
  double B2 = refl_coeffs[10]; // 0.000242;//0.242*1e-3;
  double C2 = refl_coeffs[11]; // -0.00000146;//-1.46*1e-6;
  double D2 = refl_coeffs[12]; 
  double E2 = refl_coeffs[13]; 
  double F2 = refl_coeffs[14]; 
  double G2 = refl_coeffs[15]; 
  
  double L1 = O1 + A1*deg_alpha + B1*deg_alpha*deg_alpha + C1*deg_alpha*deg_alpha*deg_alpha;
  double K1 = D1 + E1*deg_alpha + F1*deg_alpha*deg_alpha + G1*deg_alpha*deg_alpha*deg_alpha;
  if (K1 == 0) K1 = 1;
    
  double L2 = O2 + A2*deg_alpha + B2*deg_alpha*deg_alpha + C2*deg_alpha*deg_alpha*deg_alpha;
  double K2 = D2 + E2*deg_alpha + F2*deg_alpha*deg_alpha + G2*deg_alpha*deg_alpha*deg_alpha;
  if (K2 == 0) K2 = 1;
  
  reflectance = 2*L1*mu_0/(mu_0+mu)/K1 + (1-L2)*mu_0/K2;
  
  if (mu_0 + mu == 0 || reflectance != reflectance)
    return 0.0;

  // Attempt to compensate for points on the terrain being too bright
  // if the sun is behind the spacecraft as seen from those points.
  reflectance *= ( exp(-phaseCoeffC1*alpha) + phaseCoeffC2 );

  return reflectance;
}

double MMPFReflectance(Vector3 const& sunPos,
                       Vector3 const& viewPos,
                       Vector3 const& xyz,
                       Vector3 const& normal,
                       double phaseCoeffC1,
                       double phaseCoeffC2,
                       double & alpha,
                       const double * refl_coeffs) {

    double reflectance;
    double LS;

    double len = dot_prod(normal, normal);
    if (abs(len - 1.0) > 1.0e-4){
        std::cerr << "Error: Expecting unit normal in the reflectance computation, in "
                << __FILE__ << " at line " << __LINE__ << std::endl;
        exit(1);
    }

    // Compute sun and view direction vectors
    Vector3 sunDirection = normalize(sunPos - xyz);
    Vector3 viewDirection = normalize(viewPos - xyz);

    // Compute cosine of incendence (i) and emission (e) angles
    double cos_i = dot_prod(sunDirection, normal);
    double cos_e = dot_prod(viewDirection, normal);

    // Compute phase angle (g) in degrees
    double cos_g = dot_prod(sunDirection, viewDirection);
    
    if ((cos_g > 1) || (cos_g < -1)) {
        printf("cos_g error\n");
    }

    double g_rad = acos(cos_g);  // phase angle in radians
    double g = g_rad * 180 / M_PI; // phase angle in degrees

    // Apply Lommel-Seeliger correction
    if (cos_i + cos_e == 0.0) { // Avoid dividing by zero
        return 0.0;
    }

    LS = cos_i / (cos_i + cos_e);

    // Mature Highlands Coeffs
    double a0 = refl_coeffs[0];
    double a1 = refl_coeffs[1];
    double a2 = refl_coeffs[2];
    double a3 = refl_coeffs[3];
    double a4 = refl_coeffs[4];
    double a5 = refl_coeffs[5];
    double a6 = refl_coeffs[6];

    // Compute reflectance
    reflectance = LS * exp(a0 
                         + a1 * g * g
                         + a2 * g
                         + a3 * sqrt(g)
                         + a4 * cos_e
                         + a5 * cos_i
                         + a6 * cos_i * cos_i);
    
    return reflectance;
}

// Computes the ground reflectance with a desired reflectance model.
double calcReflectance(vw::Vector3 const& cameraPosition,
                       vw::Vector3 const& normal, Vector3 const& xyz,
                       vw::Vector3 const& sun_position,
                       ReflParams const& refl_params,
                       const double * refl_coeffs) {

  double phase_angle = 0.0;

  double input_img_reflectance = 0.0;

  switch (refl_params.reflectanceType) {
    case LUNAR_LAMBERT:
      input_img_reflectance
        = LunarLambertianReflectance(sun_position,
                                     cameraPosition,
                                     xyz,  normal,
                                     refl_params.phaseCoeffC1,
                                     refl_params.phaseCoeffC2,
                                     phase_angle, // output
                                     refl_coeffs);
      break;
    case ARBITRARY_MODEL:
      input_img_reflectance
        = ExperimentalLunarLambertianReflectance(sun_position,
                                                 cameraPosition,
                                                 xyz,  normal,
                                                 refl_params.phaseCoeffC1,
                                                 refl_params.phaseCoeffC2,
                                                 phase_angle, // output
                                                 refl_coeffs);
      break;
    case HAPKE:
      input_img_reflectance
        = HapkeReflectance(sun_position,
                           cameraPosition,
                           xyz,  normal,
                           refl_params.phaseCoeffC1,
                           refl_params.phaseCoeffC2,
                           phase_angle, // output
                           refl_coeffs);
      break;
    case CHARON:
      input_img_reflectance
        = CharonReflectance(sun_position,
                            cameraPosition,
                            xyz,  normal,
                            refl_params.phaseCoeffC1,
                            refl_params.phaseCoeffC2,
                            phase_angle, // output
                            refl_coeffs);
      break;
    case LAMBERT:
      input_img_reflectance = LambertianReflectance(sun_position, xyz, normal);
      break;
    case MMPF:
      input_img_reflectance = MMPFReflectance(sun_position,
                            cameraPosition,
                            xyz,  normal,
                            refl_params.phaseCoeffC1,
                            refl_params.phaseCoeffC2,
                            phase_angle, // output
                            refl_coeffs);
      break;
    default:
      input_img_reflectance = 1;
    }

  return input_img_reflectance;
}

// Computed intensity: 
// albedo * nonlinReflectance(reflectance_i, exposures[i], haze, num_haze_coeffs) + haze[0]
// Cost function:
// sum_i | I_i - comp_intensity_i|^2
double calcIntensity(double albedo, double reflectance, double exposure, 
                     double steepness_factor, double const* haze, int num_haze_coeffs) {
  return albedo 
  * nonlinReflectance(reflectance, exposure, steepness_factor, haze, num_haze_coeffs)
  + haze[0]; 
}

// Calc albedo given the intensity.  
// albedo = (intensity - haze[0]) / nonlin_ref. 
// See also calcIntensity().
double calcAlbedo(double intensity, double reflectance, double exposure, 
                  double steepness_factor, double const* haze, int num_haze_coeffs) {
 
  // First, subtract the base haze coefficient
  double adjusted_intensity = intensity - haze[0];
  
  // Calculate the nonlinear reflectance first
  double nonlin_ref = nonlinReflectance(reflectance, exposure, 
                                        steepness_factor, haze, num_haze_coeffs);
  
  // Protect against division by zero
  if (nonlin_ref == 0.0)
      return 0.0;
  
  return adjusted_intensity / nonlin_ref;
}

// Reflectance formula that is nonlinear if there is more than one haze coefficient
// (that is experimental).
double nonlinReflectance(double reflectance, double exposure,
                         double steepness_factor,
                         double const* haze, int num_haze_coeffs) {

  // Make the exposure smaller. This will result in higher reflectance
  // to compensate, as intensity = exposure * reflectance, hence
  // steeper terrain. Things become more complicated if the haze
  // and nonlinear reflectance is modeled. This is not on by default.
  exposure /= steepness_factor;
  
  double r = reflectance; // for short
  if (num_haze_coeffs == 0) 
    return exposure * r; // Linear model
  if (num_haze_coeffs == 1) 
    return exposure * r; // Also linear model, haze[0] is added after albedo multiplication
  if (num_haze_coeffs == 2) 
    return exposure * r /(haze[1]*r + 1);
  if (num_haze_coeffs == 3) 
    return exposure * (r + haze[2])/(haze[1]*r + 1);
  if (num_haze_coeffs == 4) 
    return exposure * (haze[3]*r*r + r + haze[2])/(haze[1]*r + 1);
  if (num_haze_coeffs == 5) 
    return exposure * (haze[3]*r*r + r + haze[2])/(haze[4]*r*r + haze[1]*r + 1);
  if (num_haze_coeffs == 6) 
    return exposure * (haze[5]*r*r*r + haze[3]*r*r + r + haze[2])/(haze[4]*r*r + haze[1]*r + 1);
    
  vw_throw(ArgumentErr() << "Invalid value for the number of haze coefficients.\n");
  return 0;
}

// Calculate current ECEF position and normal vector for a given DEM pixel.
// This is an auxiliary function needed to compute the reflectance.
void calcPointAndNormal(int col, int row,
                        double left_h, double center_h, double right_h,
                        double bottom_h, double top_h,
                        bool use_pq, double p, double q, // dem partial derivatives
                        vw::cartography::GeoReference const& geo,
                        double gridx, double gridy,
                        // Outputs
                        vw::Vector3 & xyz, vw::Vector3 & normal) {

  if (use_pq) {
    // p is defined as (right_h - left_h)/(2*gridx)
    // so, also, p = (right_h - center_h)/gridx
    // Hence, we get the formulas below in terms of p and q.
    right_h  = center_h + gridx*p;
    left_h   = center_h - gridx*p;
    top_h    = center_h + gridy*q;
    bottom_h = center_h - gridy*q;
  }

  // The xyz position at the center grid point
  vw::Vector2 lonlat = geo.pixel_to_lonlat(Vector2(col, row));
  double h = center_h;
  vw::Vector3 lonlat3(lonlat(0), lonlat(1), h);
  xyz = geo.datum().geodetic_to_cartesian(lonlat3);

  // The xyz position at the left grid point
  lonlat = geo.pixel_to_lonlat(Vector2(col-1, row));
  h = left_h;
  lonlat3 = vw::Vector3(lonlat(0), lonlat(1), h);
  vw::Vector3 left = geo.datum().geodetic_to_cartesian(lonlat3);

  // The xyz position at the right grid point
  lonlat = geo.pixel_to_lonlat(Vector2(col+1, row));
  h = right_h;
  lonlat3 = vw::Vector3(lonlat(0), lonlat(1), h);
  vw::Vector3 right = geo.datum().geodetic_to_cartesian(lonlat3);

  // The xyz position at the bottom grid point
  lonlat = geo.pixel_to_lonlat(Vector2(col, row+1));
  h = bottom_h;
  lonlat3 = vw::Vector3(lonlat(0), lonlat(1), h);
  vw::Vector3 bottom = geo.datum().geodetic_to_cartesian(lonlat3);

  // The xyz position at the top grid point
  lonlat = geo.pixel_to_lonlat(Vector2(col, row-1));
  h = top_h;
  lonlat3 = vw::Vector3(lonlat(0), lonlat(1), h);
  vw::Vector3 top = geo.datum().geodetic_to_cartesian(lonlat3);

  // four-point normal (centered)
  vw::Vector3 dx = right - left;
  vw::Vector3 dy = bottom - top;

  normal = -normalize(cross_prod(dx, dy)); // so normal points up
}  

} // end namespace asp
