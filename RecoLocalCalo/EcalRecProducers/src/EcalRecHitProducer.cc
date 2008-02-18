/** \class EcalRecHitProducer
 *   produce ECAL rechits from uncalibrated rechits
 *
 *  $Id: EcalRecHitProducer.cc,v 1.17 2008/02/18 11:38:49 ferriff Exp $
 *  $Date: 2008/02/18 11:38:49 $
 *  $Revision: 1.17 $
 *  \author Shahram Rahatlou, University of Rome & INFN, March 2006
 *
 **/
#include "RecoLocalCalo/EcalRecProducers/interface/EcalRecHitProducer.h"
#include "RecoLocalCalo/EcalRecAlgos/interface/EcalRecHitSimpleAlgo.h"
#include "CondFormats/EcalObjects/interface/EcalIntercalibConstants.h"
#include "CondFormats/DataRecord/interface/EcalIntercalibConstantsRcd.h"
#include "CondFormats/EcalObjects/interface/EcalADCToGeVConstant.h"
#include "CondFormats/DataRecord/interface/EcalADCToGeVConstantRcd.h"
#include "CondFormats/EcalObjects/interface/EcalChannelStatus.h"
#include "CondFormats/DataRecord/interface/EcalChannelStatusRcd.h"
#include "CalibCalorimetry/EcalLaserCorrection/interface/EcalLaserDbService.h"
#include "CalibCalorimetry/EcalLaserCorrection/interface/EcalLaserDbRecord.h"

#include "DataFormats/Common/interface/Handle.h"
#include "FWCore/Framework/interface/ESHandle.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include <iostream>
#include <cmath>
#include <vector>

//#include "CondFormats/EcalObjects/interface/EcalPedestals.h"
//#include "CondFormats/DataRecord/interface/EcalPedestalsRcd.h"
#include "DataFormats/EcalRecHit/interface/EcalUncalibratedRecHit.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHit.h"
#include "DataFormats/EcalDetId/interface/EBDetId.h"
#include "DataFormats/EcalDetId/interface/EEDetId.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHitCollections.h"



EcalRecHitProducer::EcalRecHitProducer(const edm::ParameterSet& ps) {

   EBuncalibRecHitCollection_ = ps.getParameter<edm::InputTag>("EBuncalibRecHitCollection");
   EEuncalibRecHitCollection_ = ps.getParameter<edm::InputTag>("EEuncalibRecHitCollection");
   EBrechitCollection_        = ps.getParameter<std::string>("EBrechitCollection");
   EErechitCollection_        = ps.getParameter<std::string>("EErechitCollection");
   //   nMaxPrintout_            = ps.getUntrackedParameter<int>("nMaxPrintout",10);

   EBalgo_ = new EcalRecHitSimpleAlgo();
   EEalgo_ = new EcalRecHitSimpleAlgo();

   produces< EBRecHitCollection >(EBrechitCollection_);
   produces< EERecHitCollection >(EErechitCollection_);

   v_chstatus_ = ps.getParameter<std::vector<int> >("ChannelStatusToBeExcluded");
   //   nEvt_ = 0; // reset local event counter
}

EcalRecHitProducer::~EcalRecHitProducer() {

  if (EBalgo_) delete EBalgo_;
  if (EEalgo_) delete EEalgo_;

}

void
EcalRecHitProducer::produce(edm::Event& evt, const edm::EventSetup& es) {

   using namespace edm;

   //   nEvt_++;

   Handle< EBUncalibratedRecHitCollection > pEBUncalibRecHits;
   Handle< EEUncalibratedRecHitCollection > pEEUncalibRecHits;

   const EBUncalibratedRecHitCollection*  EBuncalibRecHits = 0;
   const EEUncalibratedRecHitCollection*  EEuncalibRecHits = 0; 

   if ( EBuncalibRecHitCollection_.label() != "" && EBuncalibRecHitCollection_.instance() != "" ) {
           evt.getByLabel( EBuncalibRecHitCollection_, pEBUncalibRecHits);
           if ( pEBUncalibRecHits.isValid() ) {
                   EBuncalibRecHits = pEBUncalibRecHits.product(); // get a ptr to the product
#ifdef DEBUG
                   LogDebug("EcalRecHitDebug") << "total # EB uncalibrated rechits: " << EBuncalibRecHits->size();
#endif
           } else {
                   edm::LogError("EcalRecHitError") << "Error! can't get the product " << EBuncalibRecHitCollection_;
           }
   }

   if ( EEuncalibRecHitCollection_.label() != "" && EEuncalibRecHitCollection_.instance() != "" ) {
           evt.getByLabel( EEuncalibRecHitCollection_, pEEUncalibRecHits);
           if ( pEEUncalibRecHits.isValid() ) {
                   EEuncalibRecHits = pEEUncalibRecHits.product(); // get a ptr to the product
#ifdef DEBUG
                   LogDebug("EcalRecHitDebug") << "total # EE uncalibrated rechits: " << EEuncalibRecHits->size();
#endif
           } else {
                   edm::LogError("EcalRecHitError") << "Error! can't get the product " << EEuncalibRecHitCollection_;
           }
   }

   // collection of rechits to put in the event
   std::auto_ptr< EBRecHitCollection > EBrechits( new EBRecHitCollection );
   std::auto_ptr< EERecHitCollection > EErechits( new EERecHitCollection );

   // now fetch all conditions we nEEd to make rechits
   edm::ESHandle<EcalADCToGeVConstant> pAgc;
   es.get<EcalADCToGeVConstantRcd>().get(pAgc);
   const EcalADCToGeVConstant* agc = pAgc.product();
   //
   // use this value in the algorithm
   EBalgo_->setADCToGeVConstant(float(agc->getEBValue()));
   EEalgo_->setADCToGeVConstant(float(agc->getEEValue()));
   // Intercalib constants
   edm::ESHandle<EcalIntercalibConstants> pIcal;
   es.get<EcalIntercalibConstantsRcd>().get(pIcal);
   const EcalIntercalibConstants* ical = pIcal.product();
   const EcalIntercalibConstantMap& icalMap=ical->getMap();
#ifdef DEBUG
   LogDebug("EcalRecHitDebug") << "Global EB ADC->GeV scale: " << agc->getEBValue() << " GeV/ADC count" ;
#endif
   // ADC -> GeV Scale

   edm::ESHandle<EcalLaserDbService> pLaser;
   es.get<EcalLaserDbRecord>().get( pLaser );

   // channel status map
   edm::ESHandle<EcalChannelStatus> pChStatus;
   es.get<EcalChannelStatusRcd>().get( pChStatus );
   const EcalChannelStatus* chStatus = pChStatus.product();

   if (EBuncalibRecHits)
     {
       
       // loop over uncalibrated rechits to make calibrated ones
       for(EBUncalibratedRecHitCollection::const_iterator it  = EBuncalibRecHits->begin(); it != EBuncalibRecHits->end(); ++it) {
	 
         // check if status flag is valid -- FIXME: specialize flags
	 EcalChannelStatusMap::const_iterator chit = chStatus->find(it->id());
         EcalChannelStatusCode chStatusCode = 1;
         if ( chit != chStatus->end() ) {
                 chStatusCode = *chit;
         } else {
                 edm::LogError("EcalRecHitError") << "No channel status found for xtal " << EBDetId(it->id()) << "! something wrong with EcalChannelStatus in your DB? ";
         }
         if ( v_chstatus_.size() > 0) {
                 std::vector<int>::const_iterator res = std::find( v_chstatus_.begin(), v_chstatus_.end(), chStatusCode.getStatusCode() );
                 if ( *res ) {
                         continue;
                 }
         }

	 // find intercalib constant for this xtal
	 EcalIntercalibConstantMap::const_iterator icalit=icalMap.find(it->id());
         EcalIntercalibConstant icalconst = 1;
	 if( icalit!=icalMap.end() ){
	   icalconst = (*icalit);
	   //	   LogDebug("EcalRecHitDebug") << "Found intercalib for xtal " << EBDetId(it->id()).ic() << " " << icalconst ;
	 } else {
	   edm::LogError("EcalRecHitError") << "No intercalib const found for xtal " << EBDetId(it->id()) << "! something wrong with EcalIntercalibConstants in your DB? " ;
	 }
         // get laser coefficient
         float lasercalib = pLaser->getLaserCorrection( EBDetId(it->id()), evt.time() );
	 
	 // make the rechit and put in the output collection
	 // must implement op= for EcalRecHit
	 EcalRecHit aHit( EBalgo_->makeRecHit(*it, icalconst * lasercalib) );
	 EBrechits->push_back( aHit );
	 
#ifdef DEBUG	 
	 if(it->amplitude()>0.) 
	   {
	     LogDebug("EcalRecHitDebug") << "processed UncalibRecHit with rawId: "
					     << it->id().rawId() << "\n"
					     << "uncalib rechit amplitude: " << it->amplitude()
					     << " calib rechit energy: " << aHit.energy()
	       ;
	   }
#endif
       }
     }

   if (EEuncalibRecHits)
     {
       // loop over uncalibrated rechits to make calibrated ones
       for(EEUncalibratedRecHitCollection::const_iterator it  = EEuncalibRecHits->begin(); it != EEuncalibRecHits->end(); ++it) {
	 
         // check if status flag is valid -- FIXME: specialize flags
	 EcalChannelStatusMap::const_iterator chit = chStatus->find(it->id());
         EcalChannelStatusCode chStatusCode = 1;
         if ( chit != chStatus->end() ) {
                 chStatusCode = *chit;
         } else {
                 edm::LogError("EcalRecHitError") << "No channel status found for xtal " << EBDetId(it->id()) << "! something wrong with EcalChannelStatus in your DB? ";
         }
         if ( v_chstatus_.size() > 0) {
                 std::vector<int>::const_iterator res = std::find( v_chstatus_.begin(), v_chstatus_.end(), chStatusCode.getStatusCode() );
                 if ( *res ) {
                         continue;
                 }
         }

	 // find intercalib constant for this xtal
	 EcalIntercalibConstantMap::const_iterator icalit=icalMap.find(it->id());
	 EcalIntercalibConstant icalconst = 1;
	 if( icalit!=icalMap.end() ){
	   icalconst = (*icalit);
	   // LogDebug("EcalRecHitDebug") << "Found intercalib for xtal " << EEDetId(it->id()).ic() << " " << icalconst ;
	 } else {
	   edm::LogError("EcalRecHitError") << "No intercalib const found for xtal " << EEDetId(it->id()) << "! something wrong with EcalIntercalibConstants in your DB? "
	     ;
	 }
         // get laser coefficient
         float lasercalib = pLaser->getLaserCorrection( EEDetId(it->id()), evt.time() );
	 
	 // make the rechit and put in the output collection
	 // must implement op= for EcalRecHit
	 EcalRecHit aHit( EEalgo_->makeRecHit(*it, icalconst * lasercalib) );
	 EErechits->push_back( aHit );
	 
#ifdef DEBUG
	 if(it->amplitude()>0.) 
	   {
	     LogDebug("EcalRecHitDebug") << "processed UncalibRecHit with rawId: "
					     << it->id().rawId() << "\n"
					     << "uncalib rechit amplitude: " << it->amplitude()
					     << " calib rechit energy: " << aHit.energy()
	       ;
	   }
#endif
       }
     }
   // put the collection of recunstructed hits in the event   
   LogInfo("EcalRecHitInfo") << "total # EB calibrated rechits: " << EBrechits->size()
     ;
   LogInfo("EcalRecHitInfo") << "total # EE calibrated rechits: " << EErechits->size()
     ;

   evt.put( EBrechits, EBrechitCollection_ );
   evt.put( EErechits, EErechitCollection_ );
} //produce()

