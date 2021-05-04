from RecoPixelVertexing.PixelTriplets.pixelTripletLargeTipEDProducerDefault_cfi import pixelTripletLargeTipEDProducerDefault as _pixelTripletLargeTipEDProducerDefault

pixelTripletLargeTipEDProducer = _pixelTripletLargeTipEDProducerDefault.clone()
from Configuration.Eras.Modifier_trackingLowPU_cff import trackingLowPU
from Configuration.Eras.Modifier_trackingPhase2PU140_cff import trackingPhase2PU140
trackingLowPU.toModify(pixelTripletLargeTipEDProducer, maxElement=100000)
trackingPhase2PU140.toModify(pixelTripletLargeTipEDProducer, maxElement=0)

from Configuration.Eras.Modifier_peripheralPbPb_cff import peripheralPbPb
from Configuration.Eras.Modifier_pp_on_XeXe_2017_cff import pp_on_XeXe_2017
from Configuration.ProcessModifiers.pp_on_AA_cff import pp_on_AA
for e in [peripheralPbPb, pp_on_XeXe_2017, pp_on_AA]:
    e.toModify(pixelTripletLargeTipEDProducer, maxElement = 1000000)
