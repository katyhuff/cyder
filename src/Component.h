/** \file Component.h
 * \brief Declares the Component class used by the Generic Repository 
 * \author Kathryn D. Huff
 */
#if !defined(_COMPONENT_H)
#define _COMPONENT_H

#include <vector>
#include <map>
#include <string>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "Material.h"
#include "ThermalModel.h"
#include "NuclideModel.h"
#include "Geometry.h"

/*!
A map for storing the composition history of a material.

@TODO IsoVector should be used instead of this
*/
typedef std::map<int, std::map<int, double> > CompHistory;

/*!
A map for storing the mass history of a material.

@TODO IsoVector should be used instead of this
*/
typedef std::map<int, std::map<int, double> > MassHistory;

/// type definition for Concentrations in kg/m^3
typedef double Concentration;

/// type definition for ConcMap 
typedef std::map<int, Concentration> ConcMap;

/// type definition for Temperature in Kelvin 
typedef double Temp;

/// type definition for Power in Watts
typedef double Power;

/// Enum for type of engineered barrier component.
enum ComponentType {BUFFER, ENV, FF, NF, WF, WP, LAST_EBS};

/** 
   @brief Defines interface for subcomponents of the GenericRepository
   
   Components such as the Waste Form, Waste Package, Buffer, Near Field,
   Far Field, and Envrionment will share a universal interface so that 
   information passing concerning fluxes and other boundary conditions 
   can be passed in and out of them.
 */
class Component {

public:
  /**
     Default constructor for the component class. Creates an empty component.
   */
  Component();

  /** 
     Default destructor does nothing.
   */
  ~Component();

  /**
     initializes the model parameters from an xmlNodePtr
     and calls the explicit init(ID, name, ...) function. 
     
     @param cur is the current xmlNodePtr
   */
  void init(xmlNodePtr cur); 

  /**
     initializes the model parameters from an xmlNodePtr
     
     @param name  the name_ data member, a string
     @param type the type_ data member, a ComponentType enum value
     @param inner_radius the inner_radius_ data member, in meters
     @param outer_radius the outer_radius_ data member, in meters 
     @param thermal_model the thermal_model_ data member, a pointer
     @param nuclide_model the nuclide_model_ data member, a pointer
   */
  void init(std::string name, ComponentType type, Radius inner_radius,
      Radius outer_radius, ThermalModel* thermal_model, NuclideModel* nuclide_model); 
  /**
     initializes the model parameters from an xmlNodePtr
     
     @param name  the name_ data member, a string
     @param type the type_ data member, a ComponentType enum value
     @param geom the geom_ data member, a Geometry object shared pointer
     @param thermal_model the thermal_model_ data member, a pointer
     @param nuclide_model the nuclide_model_ data member, a pointer
   */
  void init(std::string name, ComponentType type, GeometryPtr geom,
     ThermalModel* thermal_model, NuclideModel* nuclide_model); 

  /**
     copies a component and its parameters from another
     
     @param src is the component being copied
   */
  void copy(Component* src); 

  /**
     standard verbose printer includes current temp and concentrations
   */
  void print(); 

  /**
     Absorbs the contents of the given Material into this Component.
     
     @param mat_to_add the Material to be absorbed
   */
  void absorb(mat_rsrc_ptr mat_to_add);

  /**
     Extracts the contents of the given Material from this Component. Use this 
     function for decrementing a Component's mass balance after transferring 
     through a link. 
     
     @param mat_to_rem the Material whose composition we want to decrement 
     against this Component
   */
  void extract(mat_rsrc_ptr mat_to_rem);

  /**
     Transports heat from the inner boundary to the outer boundary in this 
     component
   */
  void transportHeat();

  /**
     Transports nuclides from the inner boundary to the outer boundary in this 
     component
   */
  void transportNuclides();

  /** 
     Loads this component with another component.
     
     @param type the ComponentType of this component
     @param to_load the Component to load into this component
   */
  Component* load(ComponentType type, Component* to_load);

  /**
     Reports true if the component is full of whatever goes inside it.
     
     @return TRUE if the component is full and FALSE if there is no more room 
   */
  bool isFull() ;

  /**
     Returns the ComponentType of this component (WF, WP, etc.)
     
     @return type_ the ComponentType of this component
   */
  ComponentType type();

  /**
     Enumerates a string if it is one of the named ComponentTypes
     
     @param type the name of the ComponentType (i.e. FF)
     @return the ComponentType enum associated with this string by the 
     component_type_names_ list 
   */
  ComponentType componentEnum(std::string type);
  
  /**
     Enumerates a string if it is one of the named ThermalModelTypes
     
     @param type the name of the ThermalModelType (i.e. StubThermal)
   */
  ThermalModelType thermalEnum(std::string type);

  /** 
     Enumerates a string if it is one of the named NuclideModelTypes
     
     @param type the name of the NuclideModelType (i.e. StubNuclide)
   */
  NuclideModelType nuclideEnum(std::string type);

  /** 
     Returns a new thermal model of the string type xml node pointer
     
     @param cur the xml node pointer defining the thermal model
     @return thermal_model_ a pointer to the ThermalModel that was created
   */
  ThermalModel* thermal_model(xmlNodePtr cur);

  /** 
     Returns a new nuclide model of the string type and xml node pointer
     
     @param cur the xml node pointer defining the nuclide model
     @return nuclide_model_ a pointer to the NuclideModel that was created
   */
  NuclideModel* nuclide_model(xmlNodePtr cur);

  /** 
     Returns a new thermal model that is a copy of the src model
     
     @param src a pointer to the thermal model to copy
     @return a pointer to the ThermalModel that was created
   */
  ThermalModel* copyThermalModel(ThermalModel* src);

  /** 
     Returns atd::s new nuclide model that is a copy of the src model
   */
  NuclideModel* copyNuclideModel(NuclideModel* src);

  /**
     get the ID
     
     @return ID_
   */
  const int ID();

  /**
     get the Name
     
     @return name_
   */
  const std::string name();
 
  /**
     get the list of daughter components 
     
     @return components
   */
  const std::vector<Component*> daughters();

  /**
     get the parent component 
     
     @return component
   */
  Component* parent();

  /**
     get the list of waste objects 
     
     @return wastes
   */
  const std::vector<mat_rsrc_ptr> wastes();

  /**
     get the maximum Temperature this object allows at its boundaries 
     
     @return temp_lim_
   */
  const Temp temp_lim();

  /**
     get the maximum Toxicity this object allows at its boundaries 
     
     @return tox_lim_
   */
  const Tox tox_lim();

  /**
     get the peak Temperature this object will experience during the simulation
     
     @param type indicates whether to return the inner or outer peak temp
     
     @return peak_temp_
   */
  const Temp peak_temp(BoundaryType type);

  /**
     get the Temperature
     
     @return temp_
   */
  const Temp temp();

  /**
     get the inner radius of the object
     
     @return inner_radius_ in [meters]
   */
  const Radius inner_radius();

  /**
     get the inner radius of the object

     @return outer_radius_ in [meters]
   */
  const Radius outer_radius();

  /// get the centroid position vector of the object
  const point_t centroid();

  /// get the x component of the centroid position vector of the object
  const double x();

  /// get the y component of the centroid position vector of the object
  const double y();

  /// get the z component of the centroid position vector of the object
  const double z();

  /// get the pointer to the geometry object
  const GeometryPtr geom() {return geom_;};

  /**
     gets the pointer to the nuclide model being used in this component
     
     @return nuclide_model_
   */
  NuclideModel* nuclide_model();

  /**
     gets the pointer to the thermal model being used in this component
     
     @return thermal_model_
   */
  ThermalModel* thermal_model();

  /**
     set the parent component 
     
     @param parent is the component that should be set as the parent
   */
  void setParent(Component* parent){parent_ = parent;};

  /**
     set the placement of the object
     
     @param centroid is the centroid position vector
   */
  void setPlacement(point_t centroid){
    geom_->set_centroid(centroid);};

protected:
  /** 
     The serial number for this Component.
   */
  int ID_;

  /**
     Stores the next available component ID
   */
  static int nextID_;

  /// ThermalModleType names list
  static std::string thermal_type_names_[LAST_THERMAL];

  /// NuclideModelType names list
  static std::string nuclide_type_names_[LAST_NUCLIDE];

  /**
     The composition history of this Component, in the form of a map whose
     keys are integers representing the time at which this Component had a 
     particular composition and whose values are the corresponding 
     compositions. A composition is a map of isotopes and their 
     corresponding number of atoms.
   */
  CompHistory comp_hist_;

  /**
     The mass history of this Component, in the form of a map whose
     keys are integers representing the time at which this Component had a 
     particular composition and whose values are the corresponding mass 
     compositions. A composition is a map of isotopes and the corresponding
     masses.
   */
  MassHistory mass_hist_;

  /**
     The type of thermal model implemented by this component
   */
  ThermalModel* thermal_model_;

  /**
     The type of nuclide model implemented by this component
   */
  NuclideModel* nuclide_model_;

  /**
     The immediate parent component of this component.
   */
  Component* parent_;

  /**
     The immediate daughter components of this component.
   */
  std::vector<Component*> daughters_;

  /**
     The contained contaminants, a list of material objects..
   */
  std::vector<mat_rsrc_ptr> wastes_;

  /**
     The name of this component, a string
   */
  std::string name_;

  /**
     The geometry of the cylindrical component, a shared pointer
   */
  GeometryPtr geom_;

  /**
     The type of component that this component represents (ff, buffer, wp, etc.) 
   */
  ComponentType type_;

  /**
     The temp limit of this component 
   */
  Temp temp_lim_;

  /**
     The toxlimit of this component 
   */
  Tox tox_lim_;

  /**
     The peak temp achieved at the outer boundary 
   */
  Temp peak_outer_temp_;

  /**
     The peak temp achieved at the inner boundary 
   */
  Temp peak_inner_temp_;

  /**
     The peak tox achieved  
   */
  Tox peak_tox_;

  /**
     The temp taken to be the homogeneous temp of the whole 
     component.
   */
  Temp temp_;

  /**
     The concentrations of contaminant isotopes in the component
   */
  ConcMap concentrations_;


};



#endif