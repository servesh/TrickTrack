#ifndef TRICKTRACK_CMCELL_H 
#define TRICKTRACK_CMCELL_H 

#include <cmath>
#include <array>
#include <iostream>

#include "tricktrack/HitDoublets.h"


namespace tricktrack {


/** @class CMCellStatus 
 *
 * Internal property of a Cell acted on during evolution.
 *
 */
class CMCellStatus {

public:
  
  /// getter for the State
  unsigned char getCAState() const {
    return theCAState;
  }
  
  /// if there is at least one left neighbor with the same state (friend), the state has to be increased by 1.
  void updateState() {
    theCAState += hasSameStateNeighbors;
  }
  
  bool isRootCell(const unsigned int minimumCAState) const {
    return (theCAState >= minimumCAState);
  }
  
 public:
  unsigned char theCAState=0;
  unsigned char hasSameStateNeighbors=0;
  
};


/** @class CMCell
 *
 * Links of the chain. The cell is the smallest unit of the algorithm and 
 * carries indices referring to the doublet and input hits. Tracklets are built
 * by updating its state.
 * */
template <typename Hit>
class CMCell {
public:
  using CMntuple = std::vector<unsigned int>;
  using CMntuplet = std::vector<unsigned int>;
  using CMColl = std::vector<CMCell<Hit>>;
  using CAStatusColl = std::vector<CMCellStatus>;
  
  
  CMCell(const HitDoublets<Hit>* doublets, int doubletId, const int innerHitId, const int outerHitId) :
    theDoublets(doublets), theDoubletId(doubletId)
    ,theInnerR(doublets->rv(doubletId, HitDoublets<Hit>::inner)) 
    ,theInnerZ(doublets->z(doubletId, HitDoublets<Hit>::inner))
  {}

   
  
  Hit const & getInnerHit() const {
    return theDoublets->hit(theDoubletId, HitDoublets<Hit>::inner);
  }
  
  Hit const & getOuterHit() const {
    return theDoublets->hit(theDoubletId, HitDoublets<Hit>::outer);
  }
  
  
  float getInnerX() const {
    return theDoublets->x(theDoubletId, HitDoublets<Hit>::inner);
  }
  
  float getOuterX() const {
    return theDoublets->x(theDoubletId, HitDoublets<Hit>::outer);
  }
  
  float getInnerY() const {
    return theDoublets->y(theDoubletId, HitDoublets<Hit>::inner);
  }
  
  float getOuterY() const {
    return theDoublets->y(theDoubletId, HitDoublets<Hit>::outer);
  }
  
  float getInnerZ() const {
    return theInnerZ;
  }
  
  float getOuterZ() const {
    return theDoublets->z(theDoubletId, HitDoublets<Hit>::outer);
  }
  
  float getInnerR() const {
    return theInnerR;
  }
  
  float getOuterR() const {
    return theDoublets->rv(theDoubletId, HitDoublets<Hit>::outer);
  }
  
  float getInnerPhi() const {
    return theDoublets->phi(theDoubletId, HitDoublets<Hit>::inner);
  }
  
  float getOuterPhi() const {
    return theDoublets->phi(theDoubletId, HitDoublets<Hit>::outer);
  }
  
  /// local action undertaken during CM evolution:
  /// the state is increased if the cell has neighbors with the same state
  void evolve(unsigned int me, CAStatusColl& allStatus) {
    
    allStatus[me].hasSameStateNeighbors = 0;
    auto mystate = allStatus[me].theCAState;
    
    for (auto oc : theOuterNeighbors ) {
      
      if (allStatus[oc].getCAState() == mystate) {
	
	allStatus[me].hasSameStateNeighbors = 1;
	
	break;
      }
    }
    
  }
  
  void checkAlignmentAndAct(CMColl& allCells, CMntuple & innerCells, const float ptmin, const float region_origin_x,
			    const float region_origin_y, const float region_origin_radius, const float thetaCut,
			    const float phiCut, const float hardPtCut, std::vector<CMCell::CMntuplet> * foundTriplets) {
    int ncells = innerCells.size();
    int constexpr VSIZE = 16;
    int ok[VSIZE];
    float r1[VSIZE];
    float z1[VSIZE];
    auto ro = getOuterR();
    auto zo = getOuterZ();
    unsigned int cellId = this - &allCells.front();
    auto loop = [&](int i, int vs) {
      for (int j=0;j<vs; ++j) {
	auto koc = innerCells[i+j];
	auto & oc =  allCells[koc];
	r1[j] = oc.getInnerR();
	z1[j] = oc.getInnerZ();
      }
      // this vectorize!
      for (int j=0;j<vs; ++j) ok[j] = areAlignedRZ(r1[j], z1[j], ro, zo, ptmin, thetaCut);
      for (int j=0;j<vs; ++j) {
	auto koc = innerCells[i+j];
	auto & oc =  allCells[koc]; 
	 if (ok[j]&&haveSimilarCurvature(oc,ptmin, region_origin_x, region_origin_y,
					region_origin_radius, phiCut, hardPtCut)) {
	  if (foundTriplets) 
      foundTriplets->emplace_back(CMCell::CMntuplet{koc,cellId});
	  else {
	    oc.tagAsOuterNeighbor(cellId);
	  }
	}
      }
    };
    auto lim = VSIZE*(ncells/VSIZE);
    for (int i=0; i<lim; i+=VSIZE) loop(i, VSIZE); 
    loop(lim, ncells-lim);
    
  }
  
  void checkAlignmentAndTag(CMColl& allCells, CMntuple & innerCells, const float ptmin, const float region_origin_x,
			    const float region_origin_y, const float region_origin_radius, const float thetaCut,
			    const float phiCut, const float hardPtCut) {
    checkAlignmentAndAct(allCells, innerCells, ptmin, region_origin_x, region_origin_y, region_origin_radius, thetaCut,
			 phiCut, hardPtCut, nullptr);
    
  }
  
  void checkAlignmentAndPushTriplet(CMColl& allCells, CMntuple & innerCells, std::vector<CMCell::CMntuplet>& foundTriplets,
				    const float ptmin, const float region_origin_x, const float region_origin_y,
				    const float region_origin_radius, const float thetaCut, const float phiCut,
				    const float hardPtCut) {
    checkAlignmentAndAct(allCells, innerCells, ptmin, region_origin_x, region_origin_y, region_origin_radius, thetaCut,
			 phiCut, hardPtCut, &foundTriplets);
  }
  
  /// check cells for compatibility in the r-z plane 
  int areAlignedRZ(float r1, float z1, float ro, float zo, const float ptmin, const float thetaCut) const
  {
    float radius_diff = std::abs(r1 - ro);
    float distance_13_squared = radius_diff*radius_diff + (z1 - zo)*(z1 - zo);
    
    float pMin = ptmin*std::sqrt(distance_13_squared); //this needs to be divided by radius_diff later
    
    float tan_12_13_half_mul_distance_13_squared = fabs(z1 * (getInnerR() - ro) + getInnerZ() * (ro - r1) + zo * (r1 - getInnerR())) ;
    return tan_12_13_half_mul_distance_13_squared * pMin <= thetaCut * distance_13_squared * radius_diff;
  }
  
  
  void tagAsOuterNeighbor(unsigned int otherCell)
  {
    theOuterNeighbors.push_back(otherCell);
  }
  
  
  /// check two cells for compatibility using the curvature in x-y plane
  bool haveSimilarCurvature(const CMCell & otherCell, const float ptmin,
			    const float region_origin_x, const float region_origin_y, const float region_origin_radius, const float phiCut, const float hardPtCut) const
  {
    
     
    auto x1 = otherCell.getInnerX();
    auto y1 = otherCell.getInnerY();
    
    auto x2 = getInnerX();
    auto y2 = getInnerY();
    
    auto x3 = getOuterX();
    auto y3 = getOuterY();

    
    float distance_13_squared = (x1 - x3)*(x1 - x3) + (y1 - y3)*(y1 - y3);
    float tan_12_13_half_mul_distance_13_squared = std::abs(y1 * (x2 - x3) + y2 * (x3 - x1) + y3 * (x1 - x2)) ;
    // high pt : just straight
    if(tan_12_13_half_mul_distance_13_squared * ptmin <= 1.0e-4f*distance_13_squared)
      {
	
	float distance_3_beamspot_squared = (x3-region_origin_x) * (x3-region_origin_x) + (y3-region_origin_y) * (y3-region_origin_y);
	
	float dot_bs3_13 = ((x1 - x3)*( region_origin_x - x3) + (y1 - y3) * (region_origin_y-y3));
	float proj_bs3_on_13_squared = dot_bs3_13*dot_bs3_13/distance_13_squared;
	
	float distance_13_beamspot_squared  = distance_3_beamspot_squared -  proj_bs3_on_13_squared;
	
	    return distance_13_beamspot_squared < (region_origin_radius+phiCut)*(region_origin_radius+phiCut);
      }
    
    //87 cm/GeV = 1/(3.8T * 0.3)
    
    //take less than radius given by the hardPtCut and reject everything below
    float minRadius = hardPtCut*87.f;  // FIXME move out and use real MagField
    
    auto det = (x1 - x2) * (y2 - y3) - (x2 - x3) * (y1 - y2);
    
    
    auto offset = x2 * x2 + y2*y2;
    
    auto bc = (x1 * x1 + y1 * y1 - offset)*0.5f;
    
    auto cd = (offset - x3 * x3 - y3 * y3)*0.5f;
    
    
    
    auto idet = 1.f / det;
    
    auto x_center = (bc * (y2 - y3) - cd * (y1 - y2)) * idet;
    auto y_center = (cd * (x1 - x2) - bc * (x2 - x3)) * idet;
    
    auto radius = std::sqrt((x2 - x_center)*(x2 - x_center) + (y2 - y_center)*(y2 - y_center));
    
    if(radius < minRadius)  return false;  // hard cut on pt
    
    auto centers_distance_squared = (x_center - region_origin_x)*(x_center - region_origin_x) + (y_center - region_origin_y)*(y_center - region_origin_y);
    auto region_origin_radius_plus_tolerance = region_origin_radius + phiCut;
    auto minimumOfIntersectionRange = (radius - region_origin_radius_plus_tolerance)*(radius - region_origin_radius_plus_tolerance);
    
    if (centers_distance_squared >= minimumOfIntersectionRange) {
      auto maximumOfIntersectionRange = (radius + region_origin_radius_plus_tolerance)*(radius + region_origin_radius_plus_tolerance);
      return centers_distance_squared <= maximumOfIntersectionRange;
    } 
    
    return false;
    
  }
  
  
  // trying to free the track building process from hardcoded layers, leaving the visit of the graph
  // based on the neighborhood connections between cells.
  
  void findNtuplets(CMColl& allCells, std::vector<CMntuplet>& foundNtuplets, CMntuplet& tmpNtuplet, const unsigned int minHitsPerNtuplet) const {
    
    // the building process for a track ends if:
    // it has no outer neighbor
    // it has no compatible neighbor
    // the ntuplets is then saved if the number of hits it contains is greater than a threshold
    
    if (tmpNtuplet.size() == minHitsPerNtuplet - 1)
      {
	foundNtuplets.push_back(tmpNtuplet);
      }
    else
      {
	unsigned int numberOfOuterNeighbors = theOuterNeighbors.size();
	for (unsigned int i = 0; i < numberOfOuterNeighbors; ++i) {
	  tmpNtuplet.push_back((theOuterNeighbors[i]));
	  allCells[theOuterNeighbors[i]].findNtuplets(allCells,foundNtuplets, tmpNtuplet, minHitsPerNtuplet);
	  tmpNtuplet.pop_back();
	}
      }
    
  }
  
  
private:
  
  CMntuple theOuterNeighbors;
  
  /// the doublet container for this layer
  const HitDoublets<Hit>* theDoublets;  
  /// the index of the cell doublet in the doublet container
  const int theDoubletId;
  
  /// cache of the z-coordinate of the doublet on the inner layer
  const float theInnerR;
  /// cache of the r-coordinate of the doublet on the inner layer
  const float theInnerZ;
  
};

} // namespace tricktrack

#endif /* TRICKTRACK_CMCELL_H */
