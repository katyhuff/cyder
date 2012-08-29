/** \file Component.cpp
 * \brief Implements the Component class used by the Generic Repository 
 * \author Kathryn D. Huff
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <time.h>

#include "CycException.h"
#include "Component.h"
#include "LumpedThermal.h"
#include "StubThermal.h"
#include "DegRateNuclide.h"
#include "LumpedNuclide.h"
#include "MixedCellNuclide.h"
#include "OneDimPPMNuclide.h"
#include "StubNuclide.h"
#include "TwoDimPPMNuclide.h"
#include "BookKeeper.h"
#include "InputXML.h"
#include "Logger.h"

using namespace std;

// Static variables to be initialized.
int Component::nextID_ = 0;

string Component::thermal_type_names_[] = {
  "LumpedThermal",
  "StubThermal"
};
string Component::nuclide_type_names_[] = {
  "DegRateNuclide",
  "LumpedNuclide",
  "MixedCellNuclide",
  "OneDimPPMNuclide",
  "StubNuclide", 
  "TwoDimPPMNuclide" 
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Component::Component(){
  name_ = "";
  type_=LAST_EBS;
  geom_ = GeometryPtr(new Geometry());
  temp_ = 0;
  temp_lim_ = 373;
  tox_lim_ = 10 ;

  thermal_model_ = NULL;
  nuclide_model_ = NULL;
  parent_ = NULL;

  comp_hist_ = CompHistory();
  mass_hist_ = MassHistory();

}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Component::~Component(){ // @TODO is there anything to delete? Make this virtual? 
  delete nuclide_model_;
  delete thermal_model_;
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Component::init(xmlNodePtr cur){

  string name = XMLinput->get_xpath_content(cur,"name");
  ComponentType type = componentEnum(XMLinput->get_xpath_content(cur,"componenttype"));
  Radius inner_radius = strtod(XMLinput->get_xpath_content(cur,"innerradius"),NULL);
  Radius outer_radius = strtod(XMLinput->get_xpath_content(cur,"outerradius"),NULL);

  LOG(LEV_DEBUG2,"GRComp") << "The Component Class init(cur) function has been called.";;

  init(name, type, inner_radius, outer_radius, thermal_model(cur), nuclide_model(cur));
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Component::init(string name, ComponentType type, 
    Radius inner_radius, Radius outer_radius, ThermalModel* thermal_model, 
    NuclideModel* nuclide_model){

  ID_=nextID_++;
  
  name_ = name;
  type_ = type;
  geom_->set_radius(INNER, inner_radius);
  geom_->set_radius(OUTER, outer_radius);

  if ( !(thermal_model) || !(nuclide_model) ) {
    string err = "The thermal or nuclide model provided is null " ;
    throw CycException(err);
  } else { 
    thermal_model->set_geom(geom_);
    nuclide_model->set_geom(geom_);

    thermal_model_ = thermal_model;
    nuclide_model_ = nuclide_model;
  }

  parent_ = NULL;

  comp_hist_ = CompHistory();
  mass_hist_ = MassHistory();

}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Component::copy(Component* src){
  ID_=nextID_++;

  name_ = src->name_;
  type_ = src->type_;

  // warning, you are currently copying the centroid as well. 
  // does this object lay on top of the one being copied?
  geom_ = src->geom()->copy(src->geom(),src->centroid());

  if ( !(src->thermal_model_) ){
    string err = "The " ;
    err += name_;
    err += " model with ID: ";
    err += src->ID_;
    err += " does not have a thermal model";
    throw CycException(err);
  } else { 
    thermal_model_ = copyThermalModel(src->thermal_model_);
  }
  if ( !(src->nuclide_model_) ) {
    string err = "The " ;
    err += name_;
    err += " model with ID: ";
    err += src->ID_;
    err += " does not have a nuclide model";
    throw CycException(err);
  }else { 
    nuclide_model_ = copyNuclideModel(src->nuclide_model_);
  }
  parent_ = NULL;

  temp_ = src->temp_;
  temp_lim_ = src->temp_lim_ ;
  tox_lim_ = src->tox_lim_ ;

  comp_hist_ = CompHistory();
  mass_hist_ = MassHistory();

}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void Component::print(){
  LOG(LEV_DEBUG2,"GRComp") << "Component: " << this->name();
  LOG(LEV_DEBUG2,"GRComp") << "Contains Materials:";
  for(int i=0; i<this->wastes().size() ; i++){
    LOG(LEV_DEBUG2,"GRComp") << wastes_[i];
  }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Component::absorb(mat_rsrc_ptr mat_to_add){
  try{
    nuclide_model_->absorb(mat_to_add);
  } catch ( exception& e ) {
    LOG(LEV_ERROR, "GRComp") << "Error occured in component absorb function." << e.what();
  }
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Component::extract(mat_rsrc_ptr mat_to_rem){
  try{
    nuclide_model_->extract(mat_to_rem);
  } catch ( exception& e ) {
    LOG(LEV_ERROR, "GRComp") << "Error occured in component extract function." << e.what();
  }
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Component::transportHeat(){
  if ( NULL == thermal_model_ ) {
    LOG(LEV_ERROR, "GRComp") << "Error, no thermal_model_ loaded before Component::transportHeat." ;
  } else {
    thermal_model_->transportHeat();
  }
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Component::transportNuclides(){
  if ( NULL == nuclide_model_ ) {
    LOG(LEV_ERROR, "GRComp") << "Error, no nuclide_model_ loaded before Component::transportNuclides." ;
  } else { 
    nuclide_model_->transportNuclides();
  }
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Component* Component::load(ComponentType type, Component* to_load) {
  to_load->setParent(this);
  daughters_.push_back(to_load);
  return this;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Component::isFull() {
  return true; // @TODO imperative, add logic here
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ComponentType Component::type(){return type_;}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ComponentType Component::componentEnum(std::string type_name) {
  ComponentType toRet = LAST_EBS;
  string component_type_names[] = {"BUFFER", "ENV", "FF", "NF", "WF", "WP"};
  for(int type = 0; type < LAST_EBS; type++){
    if(component_type_names[type] == type_name){
      toRet = (ComponentType)type;
    } 
  }
  if (toRet == LAST_EBS){
    string err_msg ="'";
    err_msg += type_name;
    err_msg += "' does not name a valid ComponentType.\n";
    err_msg += "Options are:\n";
    for(int name=0; name < LAST_EBS; name++){
      err_msg += component_type_names[name];
      err_msg += "\n";
    }
    throw CycException(err_msg);
  }
  return toRet;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ThermalModelType Component::thermalEnum(std::string type_name) {
  ThermalModelType toRet = LAST_THERMAL;
  for(int type = 0; type < LAST_THERMAL; type++){
    if(thermal_type_names_[type] == type_name){
      toRet = (ThermalModelType)type;
    } 
  }
  if (toRet == LAST_THERMAL){
    string err_msg ="'";
    err_msg += type_name;
    err_msg += "' does not name a valid ThermalModelType.\n";
    err_msg += "Options are:\n";
    for(int name=0; name < LAST_THERMAL; name++){
      err_msg += thermal_type_names_[name];
      err_msg += "\n";
    }     
    throw CycException(err_msg);
  }
  return toRet;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
NuclideModelType Component::nuclideEnum(std::string type_name) {
  NuclideModelType toRet = LAST_NUCLIDE;
  for(int type = 0; type < LAST_NUCLIDE; type++){
    if(nuclide_type_names_[type] == type_name){
      toRet = (NuclideModelType)type;
    }
  }
  if (toRet == LAST_NUCLIDE){
    string err_msg ="'";
    err_msg += type_name;
    err_msg += "' does not name a valid NuclideModelType.\n";
    err_msg += "Options are:\n";
    for(int name=0; name < LAST_NUCLIDE; name++){
      err_msg += nuclide_type_names_[name];
      err_msg += "\n";
    }
    throw CycException(err_msg);
  }
  return toRet;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
ThermalModel* Component::thermal_model(xmlNodePtr cur){
  ThermalModel* toRet;
  string model_name = XMLinput->get_xpath_name(cur,"thermalmodel/*");
  
  switch(thermalEnum(model_name))
  {
    case LUMPED_THERMAL:
      toRet = new LumpedThermal(cur);
      break;
    case STUB_THERMAL:
      toRet = new StubThermal(cur);
      break;
    default:
      throw CycException("Unknown thermal model enum value encountered."); 
  }
  return toRet;
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
NuclideModel* Component::nuclide_model(xmlNodePtr cur){
  NuclideModel* toRet;

  string model_name = XMLinput->get_xpath_name(cur,"nuclidemodel/*");

  switch(nuclideEnum(model_name))
  {
    case DEGRATE_NUCLIDE:
      toRet = new DegRateNuclide(cur);
      break;
    case LUMPED_NUCLIDE:
      toRet = new LumpedNuclide(cur);
      break;
    case MIXEDCELL_NUCLIDE:
      toRet = new MixedCellNuclide(cur);
      break;
    case ONEDIMPPM_NUCLIDE:
      toRet = new OneDimPPMNuclide(cur);
      break;
    case STUB_NUCLIDE:
      toRet = new StubNuclide(cur);
      break;
    case TWODIMPPM_NUCLIDE:
      toRet = new TwoDimPPMNuclide(cur);
      break;
    default:
      throw CycException("Unknown nuclide model enum value encountered."); 
  }
  return toRet;
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
ThermalModel* Component::copyThermalModel(ThermalModel* src){
  ThermalModel* toRet;
  switch( src->type() )
  {
    case LUMPED_THERMAL:
      toRet = new LumpedThermal();
      toRet->copy(src);
      break;
    case STUB_THERMAL:
      toRet = new StubThermal();
      toRet->copy(src);
      break;
    default:
      throw CycException("Unknown thermal model enum value encountered when copying."); 
  }      
  return toRet;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
NuclideModel* Component::copyNuclideModel(NuclideModel* src){
  NuclideModel* toRet;
  switch(src->type())
  {
    case DEGRATE_NUCLIDE:
      toRet = new DegRateNuclide();
      break;
    case LUMPED_NUCLIDE:
      toRet = new LumpedNuclide();
      break;
    case MIXEDCELL_NUCLIDE:
      toRet = new MixedCellNuclide();
      break;
    case ONEDIMPPM_NUCLIDE:
      toRet = new OneDimPPMNuclide();
      break;
    case STUB_NUCLIDE:
      toRet = new StubNuclide();
      break;
    case TWODIMPPM_NUCLIDE:
      toRet = new TwoDimPPMNuclide();
      break;
    default:
      throw CycException("Unknown nuclide model enum value encountered when copying."); 
  }      
  toRet->copy(src);
  return toRet;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const int Component::ID(){return ID_;}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const std::string Component::name(){return name_;} 

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const std::vector<Component*> Component::daughters(){return daughters_;}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
Component* Component::parent(){return parent_;}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const vector<mat_rsrc_ptr> Component::wastes(){return wastes_;}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const Temp Component::temp_lim(){return temp_lim_;}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const Tox Component::tox_lim(){return tox_lim_;}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const Temp Component::peak_temp(BoundaryType type) { 
  return (type==INNER)?peak_inner_temp_:peak_outer_temp_;}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const Temp Component::temp(){return temp_;}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const Radius Component::inner_radius(){return geom_->inner_radius();}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const Radius Component::outer_radius(){return geom_->outer_radius();}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const point_t Component::centroid(){return geom_->centroid();}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const double Component::x(){return geom_->x();}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const double Component::y(){return geom_->y();}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
const double Component::z(){return geom_->z();}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
NuclideModel* Component::nuclide_model(){return nuclide_model_;}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
ThermalModel* Component::thermal_model(){return thermal_model_;}
