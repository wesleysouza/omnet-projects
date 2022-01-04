//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "stack/mac/buffer/harq/LteHarqProcessRx.h"
#include "stack/mac/layer/LteMacBase.h"
#include "common/LteControlInfo.h"
#include "stack/mac/packet/LteHarqFeedback_m.h"
#include "stack/mac/packet/LteMacPdu.h"

using namespace omnetpp;

LteHarqProcessRx::LteHarqProcessRx(unsigned char acid, LteMacBase *owner)
{
    pdu_.resize(MAX_CODEWORDS, nullptr);
    status_.resize(MAX_CODEWORDS, RXHARQ_PDU_EMPTY);
    rxTime_.resize(MAX_CODEWORDS, 0);
    result_.resize(MAX_CODEWORDS, false);
    acid_ = acid;
    macOwner_ = owner;
    transmissions_ = 0;
    maxHarqRtx_ = owner->par("maxHarqRtx");
}

void LteHarqProcessRx::insertPdu(Codeword cw, Packet *pkt)
{

    auto pdu = pkt->peekAtFront<LteMacPdu>();
    auto lteInfo = pkt->getTag<UserControlInfo>();

    bool ndi = lteInfo->getNdi();

    EV << "LteHarqProcessRx::insertPdu - ndi is " << ndi << endl;
    if (ndi && !(status_.at(cw) == RXHARQ_PDU_EMPTY))
        throw cRuntimeError("New data arriving in busy harq process -- this should not happen");

    if (!ndi && !(status_.at(cw) == RXHARQ_PDU_EMPTY) && !(status_.at(cw) == RXHARQ_PDU_CORRUPTED))
        throw cRuntimeError(
            "Trying to insert macPdu in non-empty rx harq process: Node %d acid %d, codeword %d, ndi %d, status %d",
            macOwner_->getMacNodeId(), acid_, cw, ndi, status_.at(cw));

    // deallocate corrupted pdu received in previous transmissions
    if (pdu_.at(cw) != nullptr){
            macOwner_->dropObj(pdu_.at(cw));
            delete pdu_.at(cw);
    }

    // store new received pdu
    pdu_.at(cw) = pkt;
    result_.at(cw) = lteInfo->getDeciderResult();
    status_.at(cw) = RXHARQ_PDU_EVALUATING;
    rxTime_.at(cw) = NOW;

    transmissions_++;
}

bool LteHarqProcessRx::isEvaluated(Codeword cw)
{
    if (status_.at(cw) == RXHARQ_PDU_EVALUATING && (NOW - rxTime_.at(cw)) >= HARQ_FB_EVALUATION_INTERVAL)
        return true;
    else
        return false;
}

//LteHarqFeedback *LteHarqProcessRx::createFeedback(Codeword cw)
Packet *LteHarqProcessRx::createFeedback(Codeword cw)
{
    if (!isEvaluated(cw))
        throw cRuntimeError("Cannot send feedback for a pdu not in EVALUATING state");

    auto pduInfo = pdu_.at(cw)->getTag<UserControlInfo>();
    auto pdu = pdu_.at(cw)->peekAtFront<LteMacPdu>();

    // TODO : Change to Tag (allows length 0)
    auto fb = makeShared<LteHarqFeedback>();
    fb->setAcid(acid_);
    fb->setCw(cw);
    fb->setResult(result_.at(cw));
    fb->setFbMacPduId(pdu->getMacPduId());
    fb->setChunkLength(b(1)); // TODO: should be 0
    // fb->setByteLength(0);
    auto pkt = new Packet("harqFeedback");
    pkt->insertAtFront(fb);

    pkt->addTagIfAbsent<UserControlInfo>()->setSourceId(pduInfo->getDestId());
    pkt->addTagIfAbsent<UserControlInfo>()->setDestId(pduInfo->getSourceId());
    pkt->addTagIfAbsent<UserControlInfo>()->setFrameType(HARQPKT);

    if (!result_.at(cw))
    {
        // NACK will be sent
        status_.at(cw) = RXHARQ_PDU_CORRUPTED;

        EV << "LteHarqProcessRx::createFeedback - tx number " << (unsigned int)transmissions_ << endl;
        if (transmissions_ == (maxHarqRtx_ + 1))
        {
            EV << NOW << " LteHarqProcessRx::createFeedback - max number of tx reached for cw " << cw << ". Resetting cw" << endl;

            // purge PDU
            purgeCorruptedPdu(cw);
            resetCodeword(cw);
        }
    }
    else
    {
        status_.at(cw) = RXHARQ_PDU_CORRECT;
    }

    return pkt;
}

bool LteHarqProcessRx::isCorrect(Codeword cw)
{
    return (status_.at(cw) == RXHARQ_PDU_CORRECT);
}

Packet* LteHarqProcessRx::extractPdu(Codeword cw)
{
    if (!isCorrect(cw))
        throw cRuntimeError("Cannot extract pdu if the state is not CORRECT");

    // temporary copy of pdu pointer because reset NULLs it, and I need to return it
    auto pkt = pdu_.at(cw);
    auto pdu = pkt->peekAtFront<LteMacPdu>();
    pdu_.at(cw) = nullptr;
    resetCodeword(cw);
    
    return pkt;
}

int64_t LteHarqProcessRx::getByteLength(Codeword cw)
{
    if (pdu_.at(cw) != nullptr)
    {
        return pdu_.at(cw)->getByteLength();
    }
    else
        return 0;
}

void LteHarqProcessRx::purgeCorruptedPdu(Codeword cw)
{
    // drop ownership
    if (pdu_.at(cw) != nullptr)
        macOwner_->dropObj(pdu_.at(cw));

    delete pdu_.at(cw);
    pdu_.at(cw) = nullptr;
}

void LteHarqProcessRx::resetCodeword(Codeword cw)
{
    // drop ownership
    if (pdu_.at(cw) != nullptr){
        macOwner_->dropObj(pdu_.at(cw));
        delete pdu_.at(cw);
    }

    pdu_.at(cw) = nullptr;
    status_.at(cw) = RXHARQ_PDU_EMPTY;
    rxTime_.at(cw) = 0;
    result_.at(cw) = false;

    transmissions_ = 0;
}

LteHarqProcessRx::~LteHarqProcessRx()
{
    for (unsigned char i = 0; i < MAX_CODEWORDS; ++i)
    {
        if (pdu_.at(i) != nullptr)
        {
            cObject *mac = macOwner_;
            if (pdu_.at(i)->getOwner() == mac)
            {
                delete pdu_.at(i);
            }
            pdu_.at(i) = nullptr;
        }
    }
}

std::vector<RxUnitStatus>
LteHarqProcessRx::getProcessStatus()
{
    std::vector<RxUnitStatus> ret(MAX_CODEWORDS);

    for (unsigned int j = 0; j < MAX_CODEWORDS; j++)
    {
        ret[j].first = j;
        ret[j].second = getUnitStatus(j);
    }
    return ret;
}
