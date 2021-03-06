/*! \file LumpedNuclide.cpp
    \brief Implements the LumpedNuclide class used by the Generic Repository 
    \author Kathryn D. Huff
 */
#include <iostream>
#include <fstream>
#include <deque>
#include <time.h>
#include <boost/lexical_cast.hpp>
#include <boost/math/constants/constants.hpp>

#include "CycException.h"
#include "Logger.h"
#include "Timer.h"
#include "LumpedNuclide.h"

using namespace std;
using boost::lexical_cast;
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
LumpedNuclide::LumpedNuclide() : 
  Pe_(0),
  porosity_(0),
  v_(0),
  t_t_(0),
  formulation_(LAST_FORMULATION_TYPE) 
{ 
  set_geom(GeometryPtr(new Geometry()));
  last_updated_=0;

  Pe_ = 0;
  vec_hist_ = VecHist();
  conc_hist_ = ConcHist();
  C_0_ = IsoConcMap();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
LumpedNuclide::LumpedNuclide(QueryEngine* qe):
  Pe_(0),
  porosity_(0),
  v_(0),
  t_t_(0),
  formulation_(LAST_FORMULATION_TYPE)
{ 

  set_geom(GeometryPtr(new Geometry()));
  last_updated_=0;

  vec_hist_ = VecHist();
  conc_hist_ = ConcHist();
  C_0_ = IsoConcMap();
  initModuleMembers(qe);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
LumpedNuclide::~LumpedNuclide(){
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void LumpedNuclide::initModuleMembers(QueryEngine* qe){
  v_ = lexical_cast<double>(qe->getElementContent("advective_velocity"));
  porosity_ = lexical_cast<double>(qe->getElementContent("porosity"));
  t_t_ = lexical_cast<double>(qe->getElementContent("transit_time"));

  Pe_=NULL;

  list<string> choices;
  list<string>::iterator it;
  choices.push_back ("DM");
  choices.push_back ("EXPM");
  choices.push_back ("PFM");

  QueryEngine* formulation_qe = qe->queryElement("formulation");
  string formulation_string;
  for( it=choices.begin(); it!=choices.end(); ++it){
    if (formulation_qe->nElementsMatchingQuery(*it) == 1){
      formulation_=enumerateFormulation(*it);
      formulation_string=(*it);
    }
  }
  QueryEngine* ptr = formulation_qe->queryElement(formulation_string);
  switch(formulation_){
    case DM :
      Pe_ = lexical_cast<double>(ptr->getElementContent("peclet"));
      break;
    case EXPM :
      break;
    case PFM :
      break;
    default:
      string err = "The formulation type '"; err += formulation_;
      err += "' is not supported.";
      LOG(LEV_ERROR,"GRLNuc") << err;
      throw CycException(err);
      break;
  }

  LOG(LEV_DEBUG2,"GRLNuc") << "The LumpedNuclide Class init(cur)"
    <<" function has been called";;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
NuclideModelPtr LumpedNuclide::copy(const NuclideModel& src){
  const LumpedNuclide* src_ptr = dynamic_cast<const LumpedNuclide*>(&src);

  set_last_updated(0);
  set_Pe(src_ptr->Pe());
  set_porosity(src_ptr->porosity());
  set_formulation(src_ptr->formulation());
  set_C_0(IsoConcMap());
  v_=src_ptr->v();
  t_t_=src_ptr->t_t();

  // copy the geometry AND the centroid, it should be reset later.
  set_geom(geom_->copy(src_ptr->geom(), src_ptr->geom()->centroid()));

  wastes_ = deque<mat_rsrc_ptr>();
  vec_hist_ = VecHist();
  conc_hist_ = ConcHist();

  return shared_from_this();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void LumpedNuclide::updateNuclideParamsTable(){
  shared_from_this()->addRowToNuclideParamsTable("peclet", Pe());
  shared_from_this()->addRowToNuclideParamsTable("porosity", porosity());
  shared_from_this()->addRowToNuclideParamsTable("transit_time", t_t());
  shared_from_this()->addRowToNuclideParamsTable("advective_velocity", v());
  shared_from_this()->addRowToNuclideParamsTable("ref_disp", mat_table_->ref_disp());
  shared_from_this()->addRowToNuclideParamsTable("ref_kd", mat_table_->ref_kd());
  shared_from_this()->addRowToNuclideParamsTable("ref_sol", mat_table_->ref_sol());
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void LumpedNuclide::update(int the_time){
  assert(last_updated() <= the_time);
  update_vec_hist(the_time);
  update_conc_hist(the_time);
  set_last_updated(the_time);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void LumpedNuclide::print(){
    LOG(LEV_DEBUG2,"GRLNuc") << "LumpedNuclide Model";;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void LumpedNuclide::absorb(mat_rsrc_ptr matToAdd) {
  // Get the given LumpedNuclide's contaminant material.
  // add the material to it with the material absorb function.
  // each nuclide model should override this function
  LOG(LEV_DEBUG2,"GRLNuc") << "LumpedNuclide is absorbing material: ";
  matToAdd->print();
  wastes_.push_back(matToAdd);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
mat_rsrc_ptr LumpedNuclide::extract(const CompMapPtr comp_to_rem, double 
    kg_to_rem) {
  // Get the given LumpedNuclide's contaminant material.
  // add the material to it with the material extract function.
  // each nuclide model should override this function
  LOG(LEV_DEBUG2,"GRLNuc") << "LumpedNuclide" << "is extracting composition: ";
  comp_to_rem->print() ;
  mat_rsrc_ptr to_ret = mat_rsrc_ptr(MatTools::extract(comp_to_rem, kg_to_rem, 
        wastes_, 1e-3));
  update(last_updated());
  return to_ret;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void LumpedNuclide::transportNuclides(int the_time){
  // This should transport the nuclides through the component.
  // It will likely rely on the internal flux and will produce an external 
  // flux.  The convention will be that flux is positive in the radial 
  // direction
  // If these fluxes are negative, nuclides aphysically flow toward the waste 
  // package.
  // The LumpedNuclide class should transport all nuclides
  update(the_time);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
pair<IsoVector, double> LumpedNuclide::source_term_bc(){
  double tot_mass = 0;
  IsoConcMap conc_map = MatTools::scaleConcMap(conc_hist(last_updated()), 
      V_f());
  CompMapPtr comp_map = CompMapPtr(new CompMap(MASS));
  IsoConcMap::iterator it;
  for( it=conc_map.begin(); it!=conc_map.end(); ++it){
    (*comp_map)[(*it).first]=(*it).second;
    tot_mass += (*it).second;
  }
  IsoVector to_ret = IsoVector(comp_map);
  return make_pair(to_ret, tot_mass);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
IsoConcMap LumpedNuclide::dirichlet_bc(){
  return conc_hist(last_updated());
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
ConcGradMap LumpedNuclide::neumann_bc(IsoConcMap c_ext, Radius r_ext){
  ConcGradMap to_ret;
  IsoConcMap c_int = conc_hist(last_updated());
  Radius r_int = geom()->radial_midpoint();

  int iso;
  IsoConcMap::iterator it;
  for( it=c_int.begin(); it!=c_int.end(); ++it){
    iso=(*it).first;
    if( c_ext.count(iso) != 0 ) {
      // in both
      to_ret[iso] = calc_conc_grad(c_ext[iso], c_int[iso], r_ext, r_int);
    } else {
      // in c_int only
      to_ret[iso] = calc_conc_grad(0, c_int[iso], r_ext, r_int);
    }
  }
  for( it=c_ext.begin(); it!=c_ext.end(); ++it){
    iso = (*it).first;
    if( c_int.count(iso) == 0) {
      // in c_ext only
      to_ret[iso] = calc_conc_grad(c_ext[iso], 0, r_ext, r_int);
    }
  }
  return to_ret;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
IsoFluxMap LumpedNuclide::cauchy_bc(IsoConcMap c_ext, Radius r_ext){
  IsoFluxMap to_ret;
  ConcGradMap neumann = neumann_bc(c_ext, r_ext);

  ConcGradMap::iterator it;
  Iso iso;
  Elem elem;
  for( it = neumann.begin(); it!=neumann.end(); ++it){
    iso = (*it).first;
    elem = iso/1000;
    to_ret.insert(make_pair(iso, -mat_table_->D(elem)*(*it).second + 
          shared_from_this()->dirichlet_bc(iso)*v()));
  }
  return to_ret;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
FormulationType LumpedNuclide::enumerateFormulation(string type_name) {
  FormulationType toRet = LAST_FORMULATION_TYPE;
  string formulation_type_names[] = {"DM", "EXPM", "PFM", 
    "LAST_FORMULATION_TYPE"};
  for(int type = 0; type < LAST_FORMULATION_TYPE; type++){
    if(formulation_type_names[type] == type_name){
      toRet = (FormulationType)type;
    } }
  if (toRet == LAST_FORMULATION_TYPE){
    string err_msg ="'";
    err_msg += type_name;
    err_msg += "' does not name a valid FormulationType.\n";
    err_msg += "Options are:\n";
    for(int name=0; name < LAST_FORMULATION_TYPE; name++){
      err_msg += formulation_type_names[name];
      err_msg += "\n";
    }
    throw CycException(err_msg);
  }
  return toRet;

}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void LumpedNuclide::set_Pe(double Pe){
  // We won't allow the situation in which Pe is negative
  // as it would mean that the flow is moving toward the source, which won't 
  // happen for vertical flow
  if( Pe < 0 ) {
    stringstream msg_ss;
    msg_ss << "The LumpedNuclide Pe range is 0 to infinity, inclusive.";
    msg_ss << " The value provided was ";
    msg_ss << Pe;
    msg_ss <<  ".";
    LOG(LEV_ERROR,"GRDRNuc") << msg_ss.str();;
    throw CycRangeException(msg_ss.str());
  } else {
    Pe_ = Pe;
  }
  MatTools::validate_finite_pos((Pe));
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void LumpedNuclide::set_porosity(double porosity){
  try {
    MatTools::validate_percent(porosity);
  } catch (CycRangeException& e) {
    stringstream msg_ss;
    msg_ss << "The LumpedNuclide porosity range is 0 to 1, inclusive.";
    msg_ss << " The value provided was ";
    msg_ss << porosity;
    msg_ss <<  ".";
    LOG(LEV_ERROR,"GRDRNuc") << msg_ss.str();;
    throw CycRangeException(msg_ss.str());
  }

  porosity_ = porosity;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
double LumpedNuclide::V_ff(){
  return V_f();
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
double LumpedNuclide::V_f(){
  return MatTools::V_f(V_T(),porosity());
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
double LumpedNuclide::V_s(){
  return MatTools::V_s(V_T(),porosity());
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
double LumpedNuclide::V_T(){
  return geom_->volume();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void LumpedNuclide::update_conc_hist(int the_time, deque<mat_rsrc_ptr> mats){

  IsoConcMap to_ret;
  int dt = max(the_time - last_updated(), 1);

  pair<IsoVector, double> sum_pair; 
  sum_pair = MatTools::sum_mats(mats);

  if(sum_pair.second != 0 && V_T() > 0 && V_T() != numeric_limits<double>::infinity()) { 
    try {
      MatTools::validate_nonzero(V_T());
      MatTools::validate_finite_pos(V_T());
    } catch (CycRangeException& e) {
      stringstream msg_ss;
      msg_ss << "The LumpedNuclide requires finite, positive, nonzero volume.";
      msg_ss << " The value provided was ";
      msg_ss << geom_->volume();
      msg_ss << geom_->inner_radius();
      msg_ss << geom_->outer_radius();
      msg_ss << geom_->length();
      msg_ss <<  ".";
      LOG(LEV_ERROR,"GRLNuc") << msg_ss.str();;
      throw CycRangeException(msg_ss.str());
  }

    double scale = sum_pair.second/V_T();
    CompMapPtr curr_comp = sum_pair.first.comp();
    CompMap::const_iterator it;
    it=(*curr_comp).begin();
    while(it != (*curr_comp).end() ) {
      int iso((*it).first);
      double conc((*it).second);
      to_ret.insert(make_pair(iso, conc*scale));
      ++it;
    }
  } else {
    to_ret[ 92235 ] = 0; 
  }
  conc_hist_[the_time] = C_t(to_ret, the_time);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
IsoConcMap LumpedNuclide::C_t(IsoConcMap C_0, int the_time){
  IsoConcMap to_ret;
  switch(formulation_){
    case DM :
      to_ret = C_DM(C_0, the_time);
      break;
    case EXPM :
      to_ret = C_EXPM(C_0, the_time);
      break;
    case PFM :
      to_ret = C_PFM(C_0, the_time);
      break;
    default:
      string err = "The formulation type '"; err += formulation_;
      err += "' is not supported.";
      LOG(LEV_ERROR,"GRLNuc") << err;
      throw CycException(err);
      break;
  }
  return to_ret;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void LumpedNuclide::update_conc_hist(int the_time){
  return update_conc_hist(the_time, wastes_);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
IsoConcMap LumpedNuclide::C_DM(IsoConcMap C_0, int the_time){
  double arg = (Pe()/2.0)*(1-pow(1+4*t_t()/Pe(), 0.5));
  return MatTools::scaleConcMap(C_0, exp(arg));
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
IsoConcMap LumpedNuclide::C_EXPM(IsoConcMap C_0, int the_time){
  double scale = 1.0/(1.0+t_t());
  return MatTools::scaleConcMap(C_0, scale);
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
IsoConcMap LumpedNuclide::C_PFM(IsoConcMap C_0, int the_time){
  double scale = exp(-t_t());
  return MatTools::scaleConcMap(C_0, scale);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void LumpedNuclide::update_vec_hist(int the_time){
  vec_hist_[the_time] = MatTools::sum_mats(wastes_);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void LumpedNuclide::update_inner_bc(int the_time, std::vector<NuclideModelPtr> 
    daughters){

  std::vector<NuclideModelPtr>::iterator daughter;
  
  pair<IsoVector, double> st;
  pair<IsoVector, double> mixed;
  mixed = make_pair(IsoVector(), 0);
  Volume vol(0);
  Volume vol_sum(0);
  IsoConcMap C_0;

  if( daughters.empty() ){
    C_0[92235] = 0;
  } else {
    for( daughter = daughters.begin(); daughter!=daughters.end(); ++daughter){
      st = (*daughter)->source_term_bc() ;
      vol = (*daughter)->V_ff();
      vol_sum += vol;
      if(mixed.second == 0){
        mixed.first=st.first;
        mixed.second=st.second;
      } else {
        mixed.second +=st.second;
        mixed.first.mix(st.first,mixed.second/st.second);
      }
      // @TODO use timestep len
      absorb(mat_rsrc_ptr(extractIntegratedMass((*daughter), the_time))); 
    }
    C_0 = MatTools::comp_to_conc_map(mixed.first.comp(), mixed.second, vol_sum); 
  }
  set_C_0(C_0);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
mat_rsrc_ptr LumpedNuclide::extractIntegratedMass(NuclideModelPtr daughter, 
    double the_time){
  double pi = boost::math::constants::pi<double>();
  double del_r = geom()->outer_radius() - geom()->inner_radius();
  double len = geom()->length();
  // scalar = 2*pi*l*theta*(r_j-r_i)^2
  double scalar = daughter->V_ff();
  IsoConcMap c_i_n = daughter->dirichlet_bc();
  IsoConcMap c_j_n = C_t(C_0(), the_time);
  // m_j = scalar*(((5c_j_n/6) - (c_i_n)/3) 
  IsoConcMap c_j_scaled = MatTools::scaleConcMap(c_j_n, 5.0*scalar/6.0);
  IsoConcMap c_i_scaled = MatTools::scaleConcMap(c_i_n, 0.5*scalar);
  IsoConcMap scaled_concs = MatTools::addConcMaps(c_j_scaled, c_i_scaled);
  pair<CompMapPtr, double> to_ext = MatTools::conc_to_comp_map(scaled_concs, 1) ;

  return mat_rsrc_ptr(daughter->extract(to_ext.first, to_ext.second)); 
}

