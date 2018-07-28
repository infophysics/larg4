#include "larg4/pluginActions/MCTruthEventAction_service.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "artg4tk/services/ActionHolder_service.hh"
// Geant4  includes
#include "Geant4/G4Event.hh"
#include "Geant4/G4ParticleTable.hh"
#include "Geant4/G4IonTable.hh"
#include "Geant4/G4PrimaryVertex.hh"
#include "Geant4/G4ParticleDefinition.hh"
//
#include "nusimdata/SimulationBase/MCTruth.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "nutools/G4Base/PrimaryParticleInformation.h"
#include <iostream>
#include <cmath>
#include <CLHEP/Vector/LorentzVector.h>
using std::string;
  
G4ParticleTable* larg4::MCTruthEventActionService::fParticleTable=nullptr; 

larg4::MCTruthEventActionService::
MCTruthEventActionService(fhicl::ParameterSet const & p,
        art::ActivityRegistry &)
: PrimaryGeneratorActionBase(p.get<string>("name", "MCTruthEventActionService")),
// Initialize our message logger
logInfo_("MCTruthEventActionService")
  //fConvertMCTruth    (nullptr)
 {
  //    fConvertMCTruth = new ConvertMCTruthToG4;
}

// Destructor

larg4::MCTruthEventActionService::~MCTruthEventActionService() {

}

// Create a primary particle for an event!
// (Standard Art G4 simulation)

void larg4::MCTruthEventActionService::generatePrimaries(G4Event * anEvent) {
    // For each MCTruth (probably only one, but you never know):
    // index keeps track of which MCTruth object you are using
    size_t index = 0;
    std::map< CLHEP::HepLorentzVector, G4PrimaryVertex* >                  vertexMap;
    std::map< CLHEP::HepLorentzVector, G4PrimaryVertex* >::const_iterator  vi; 
    art::ServiceHandle<artg4tk::ActionHolderService> actionHolder;
    art::Event & evt = actionHolder -> getCurrArtEvent();
    std::vector< art::Handle< std::vector<simb::MCTruth> > > mclists;
    //    if(fInputLabels.size()==0)
      evt.getManyByType(mclists);
      //else{
      //mclists.resize(fInputLabels.size());
      //for(size_t i=0; i<fInputLabels.size(); i++)
      //  evt.getByLabel(fInputLabels[i],mclists[i]);
      // }
    //    evt.getManyByType(mclists);
    std::cout << "Primary:: MCTRUTH: Size: "<<mclists.size()<<std::endl;
    for(size_t mcl = 0; mcl < mclists.size(); ++mcl){
      art::Handle< std::vector<simb::MCTruth> > mclistHandle = mclists[mcl];
      for(size_t m = 0; m < mclistHandle->size(); ++m){
        art::Ptr<simb::MCTruth> mct(mclistHandle, m);
	std::cout << "NParticles: "<< mct->NParticles()<<std::endl;
	simb::MCParticle particle = mct->GetParticle(0);
	std::cout << "status code:  " << particle.StatusCode()<<std::endl;
	if ( particle.StatusCode() != 1 ) continue;
	// Get the Particle Data Group code for the particle.
	G4int pdgCode = particle.PdgCode();
	G4double x = particle.Vx() * CLHEP::cm;
	G4double y = particle.Vy() * CLHEP::cm;
	G4double z = particle.Vz() * CLHEP::cm;
	G4double t = particle.T()  * CLHEP::ns;
	//std::cout <<"x: "<<x
	//	  <<"  y: "<<y
	//	  <<"  z: "<<z
	//	  <<"  t: "<<t
	//	  <<std::endl;
	//std::cout << "Primary:: m  Size: "<<*(mct.get())<<std::endl;
	//simb::MCTruth GHFJ =(simb::MCTruth) *(mct.get());
	//simb::MCParticle pp = GHFJ.GetParticle(1);
	//std::cout << "pdgid  "<<pp.PdgCode()<<std::endl;

	//std::cout << "Origin::  " << GHFJ.Origin()<<std::endl;
	//std::cout << "number::  " << GHFJ.NParticles()<<std::endl;
	//	std::cout << "Origin::   Size: "<<*(mct.get()).Origin()<<std::endl;
	// Create a CLHEP four-vector from the particle's vertex.
	CLHEP::HepLorentzVector fourpos(x,y,z,t);
      
	// Is this vertex already in our map?
	G4PrimaryVertex* vertex = 0;
	std::map< CLHEP::HepLorentzVector, G4PrimaryVertex* >::const_iterator result = vertexMap.find( fourpos );
	if ( result == vertexMap.end() ){
	  // No, it's not, so create a new vertex and add it to the
	  // map.
	  vertex = new G4PrimaryVertex(x, y, z, t);
	  vertexMap[ fourpos ] = vertex;

	  // Add the vertex to the G4Event.
	  anEvent->AddPrimaryVertex( vertex );
	}
	else{
	  // Yes, it is, so use the existing vertex.
	  vertex = (*result).second;
	}
      
	// Get additional particle information.
	TLorentzVector momentum = particle.Momentum(); // (px,py,pz,E)
	TVector3 polarization = particle.Polarization();
      
	// Get the particle table if necessary.  (Note: we're
	// doing this "late" because I'm not sure at what point
	// the G4 particle table is initialized in the loading process.
	if ( fParticleTable == 0 ){
	  fParticleTable = G4ParticleTable::GetParticleTable();
	}

	// Get Geant4's definition of the particle.
	G4ParticleDefinition* particleDefinition;
      
	if(pdgCode==0){
	  particleDefinition = fParticleTable->FindParticle("opticalphoton");
	}
	else
	  particleDefinition = fParticleTable->FindParticle(pdgCode);

	if ( pdgCode > 1000000000) { // If the particle is a nucleus
	  LOG_DEBUG("ConvertPrimaryToGeant4") << ": %%% Nuclear PDG code = " << pdgCode
					      << " (x,y,z,t)=(" << x
					      << "," << y
					      << "," << z
					      << "," << t << ")"
					      << " P=" << momentum.P()
					      << ", E=" << momentum.E();
	  // If the particle table doesn't have a definition yet, ask the ion
	  // table for one. This will create a new ion definition as needed.
	  if (!particleDefinition) {
            int Z = (pdgCode % 10000000) / 10000; // atomic number
            int A = (pdgCode % 10000) / 10; // mass number
            particleDefinition = fParticleTable->GetIonTable()->GetIon(Z, A, 0.);
          }
        }

	// What if the PDG code is unknown?  This has been a known
	// issue with GENIE.
	if ( particleDefinition == 0 ){
	  LOG_DEBUG("ConvertPrimaryToGeant4") << ": %%% Code not found = " << pdgCode;
	  fUnknownPDG[ pdgCode ] += 1;
	  continue;
	}
      
	// Create a Geant4 particle to add to the vertex.
	G4PrimaryParticle* g4particle = new G4PrimaryParticle( particleDefinition,
							       momentum.Px() * CLHEP::GeV,
							       momentum.Py() * CLHEP::GeV,
							       momentum.Pz() * CLHEP::GeV);

	// Add more particle information the Geant4 particle.
	G4double charge = particleDefinition->GetPDGCharge();
	g4particle->SetCharge( charge );
	g4particle->SetPolarization( polarization.x(),
				     polarization.y(),
				     polarization.z() );
      
	// Add the particle to the vertex.
	vertex->SetPrimary( g4particle );

	// Create a PrimaryParticleInformation object, and save
	// the MCTruth pointer in it.  This will allow the
	// ParticleActionList class to access MCTruth
	// information during Geant4's tracking.
	g4b::PrimaryParticleInformation* primaryParticleInfo = new g4b::PrimaryParticleInformation;
	primaryParticleInfo->SetMCTruth( mct.get(), index );
	  
	// Save the PrimaryParticleInformation in the
	// G4PrimaryParticle for access during tracking.
	g4particle->SetUserInformation( primaryParticleInfo );

	LOG_DEBUG("ConvertPrimaryToGeant4") << ": %%% primary PDG=" << pdgCode
					    << ", (x,y,z,t)=(" << x
					    << "," << y
					    << "," << z
					    << "," << t << ")"
					    << " P=" << momentum.P()
					    << ", E=" << momentum.E();

      } // for each particle in MCTruth
      ++index;
    }

}

void larg4::MCTruthEventActionService::addG4Particle(G4Event *event,
        int pdgId,
        const G4ThreeVector& pos,
        double time,
        double energy,
        //           double properTime,
        const G4ThreeVector& mom) {
    // Create a new vertex
    //     properTime=0.0;
    G4PrimaryVertex* vertex = new G4PrimaryVertex(pos, time);

    //    static int const verbosityLevel = 0; // local debugging variable
    //    if ( verbosityLevel > 0) {
    //      cout << __func__ << " pdgId   : " <<pdgId << endl;
    //    }

    //       static bool firstTime = true; // uncomment generate all nuclei ground states
    //       if (firstTime) {
    //         G4ParticleTable::GetParticleTable()->GetIonTable()->CreateAllIon();
    //         firstTime = false;
    //       }


    G4PrimaryParticle* particle =
            new G4PrimaryParticle(pdgId,
            mom.x(),
            mom.y(),
            mom.z(),
            energy);

    // Add the particle to the event.
    vertex->SetPrimary(particle);

    // Add the vertex to the event.
    event->AddPrimaryVertex(vertex);
}

using larg4::MCTruthEventActionService;
DEFINE_ART_SERVICE(MCTruthEventActionService)