/* mac_port.c - Porting layer for the MAC to the 802.15.4 Zephyr driver */

/*
 * Copyright (c) 2025 STMicroelectronics
 *
 */


/* Includes ------------------------------------------------------------------*/
#define LOG_MODULE_NAME mac802154_stm32wba
#if defined(CONFIG_IEEE802154_DRIVER_LOG_LEVEL)
#define LOG_LEVEL CONFIG_IEEE802154_DRIVER_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_NONE
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME, LOG_LEVEL);

#include <ot_inc/instance.h>
#include <ot_inc/radio.h>
#include <zephyr/net/ieee802154_radio.h>
#include <zephyr/net/ieee802154_pkt.h>
#include "ieee802154_stm32wba.h"
#include "stm32wba_802154.h"
#include "platform.h"

/* Private includes ----------------------------------------------------------*/


/* Private defines -----------------------------------------------------------*/
#define SHORT_ADDRESS_SIZE 2
#define ACK_PKT_LENGTH 127

/* Private typedef -----------------------------------------------------------*/

typedef struct {
	struct net_pkt 			*pkt;
	struct net_buf 			*payload;
	otRadioFrame 			ack_frame;
	uint8_t				ack_psdu[ACK_PKT_LENGTH];
	otRadioFrame			*tx_frame;
	enum ieee802154_tx_mode 	mode;
	otError 			status;
	bool				isTxStarted;
	struct k_work 			tx_work_item;
	struct k_sem			tx_start_sem;
} tx_data_t;

typedef struct {
	uint8_t channel;
} rx_data_t;

typedef struct {
	const struct device 		*dev;
	struct ieee802154_radio_api 	*api;
	struct mac_cbk_dispatch_tbl 	*cbk_dispatch_tbl;
	tx_data_t 			tx;
	rx_data_t 			rx;
	bool				isAntDivEnabled;
	otInstance 			*instance;
} mac_port_context_t;

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static mac_port_context_t mac_port_ctx;


/* Global and extern variables -----------------------------------------------*/
extern otRadioFrame * allocate_radio_frame(void);

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
static int32_t ieee802154_stm_enable(struct net_if *aIfacePtr, bool aState)
{
	LOG_DBG("ieee802154_stm_enable");
	return 0;
}

static enum net_l2_flags ieee802154_stm_flags(struct net_if *aIfacePtr)
{
	return NET_L2_PROMISC_MODE | NET_L2_MULTICAST | NET_L2_MULTICAST_SKIP_JOIN_SOLICIT_NODE | NET_L2_POINT_TO_POINT;
}

static enum net_verdict ieee802154_stm_recv(struct net_if *aIfacePtr, struct net_pkt *aPktPtr)
{
	otRadioFrame recvFrame;
	memset(&recvFrame, 0, sizeof(otRadioFrame));

	LOG_DBG("ieee802154_stm_recv");

	recvFrame.mPsdu = net_buf_frag_last(aPktPtr->buffer)->data;
	recvFrame.mLength = net_buf_frags_len(aPktPtr->buffer) + IEEE802154_FCS_LENGTH;
	recvFrame.mChannel = mac_port_ctx.rx.channel;
	recvFrame.mInfo.mRxInfo.mLqi = net_pkt_ieee802154_lqi(aPktPtr);
	recvFrame.mInfo.mRxInfo.mRssi = net_pkt_ieee802154_rssi_dbm(aPktPtr);
	recvFrame.mInfo.mRxInfo.mAckedWithFramePending = net_pkt_ieee802154_ack_fpb(aPktPtr);

#if defined(CONFIG_NET_PKT_TIMESTAMP)
	recvFrame.mInfo.mRxInfo.mTimestamp = net_pkt_timestamp_ns(aPktPtr) / NSEC_PER_USEC;
#endif

	mac_port_ctx.cbk_dispatch_tbl->mac_rx_done(mac_port_ctx.instance, &recvFrame, OT_ERROR_NONE);
	net_pkt_unref(aPktPtr);

	return NET_OK;
}

static int32_t ieee802154_stm_send(struct net_if *aIfacePtr, struct net_pkt *aPktPtr)
{
	LOG_WRN("Unsupported Command received. Tx commands should come directly from MAC");
	return -1;
}

static void handle_radio_event(const struct device *aDevPtr, enum ieee802154_event aEvt, void *aEventParamsPtr)
{
	ARG_UNUSED(aEventParamsPtr);

	switch (aEvt) {
	case IEEE802154_EVENT_TX_STARTED:	
		LOG_DBG("Received IEEE802154_EVENT_TX_STARTED");
		k_sem_give(&mac_port_ctx.tx.tx_start_sem);
		mac_port_ctx.tx.isTxStarted = true;
		break;
	case IEEE802154_EVENT_RX_FAILED:
		mac_port_ctx.cbk_dispatch_tbl->mac_rx_done(mac_port_ctx.instance, NULL, OT_ERROR_FAILED);
		break;
	default:
		LOG_WRN("Unhandled event: %d", aEvt);
		/* Do nothing - ignore event */
		break;
	}

	return;
}

static void energy_detected(const struct device *aDevPtr, int16_t aMaxEd)
{
	LOG_DBG("energy_detected = %d", aMaxEd);

	if (aDevPtr == mac_port_ctx.dev) {
		mac_port_ctx.cbk_dispatch_tbl->mac_ed_scan_done(mac_port_ctx.instance, aMaxEd);
	}

	return;
}

static otError map_tx_error_to_ot_error(int32_t aTxError)
{
	otError status = OT_ERROR_NONE;

	switch (aTxError) {
	case 0:
		status = OT_ERROR_NONE;
		break;
	case -EBUSY:
		status = OT_ERROR_BUSY;
		break;
	case -ENOMSG:
		status = OT_ERROR_NO_ACK;
		break;
	case -ENOBUFS:
		status = OT_ERROR_NO_BUFS;
		break;
	case -ENOTSUP:
	case -EMSGSIZE:
		status = OT_ERROR_INVALID_ARGS;
		break;
	case -EIO:
	default:
		status = OT_ERROR_FAILED;
		break;
	}

	return status;
}

static void tx_work_handler(struct k_work *tx_work)
{
	ARG_UNUSED(tx_work);

	LOG_DBG("Processing TX work");

	int32_t aTxErr =   mac_port_ctx.api->tx(mac_port_ctx.dev,
						mac_port_ctx.tx.mode,
						mac_port_ctx.tx.pkt,
						mac_port_ctx.tx.payload);

	if (mac_port_ctx.tx.isTxStarted == false) {
		LOG_DBG("Tx start failed, aTxErr=%d", aTxErr);

		/* If Tx start failed, we need to give the semaphore to avoid deadlock in otPlatRadioTransmit()*/
		k_sem_give(&mac_port_ctx.tx.tx_start_sem);
	}
	else {
		LOG_DBG("TX done, aTxErr=%d", aTxErr);
	}

	mac_port_ctx.tx.status = map_tx_error_to_ot_error(aTxErr);

	mac_port_ctx.cbk_dispatch_tbl->mac_tx_done(mac_port_ctx.instance, mac_port_ctx.tx.tx_frame, &mac_port_ctx.tx.ack_frame, mac_port_ctx.tx.status);
}

/* Public functions ----------------------------------------------------------*/
void ieee802154_init(struct net_if *aIfacePtr)
{
	struct ieee802154_config aCfg;

	LOG_DBG("ieee802154_init");

	mac_port_ctx.dev = net_if_get_device(aIfacePtr);
	__ASSERT(mac_port_ctx.dev != NULL, "No IEEE802154 device found");
	mac_port_ctx.api = (struct ieee802154_radio_api *)mac_port_ctx.dev->api;
	__ASSERT(mac_port_ctx.api != NULL, "No IEEE802154 API found");	

	mac_port_ctx.tx.pkt = net_pkt_alloc(K_NO_WAIT);
	__ASSERT_NO_MSG(mac_port_ctx.tx.pkt != NULL);

	mac_port_ctx.tx.payload = net_pkt_get_reserve_tx_data(IEEE802154_MAX_PHY_PACKET_SIZE, K_NO_WAIT);
	__ASSERT_NO_MSG(mac_port_ctx.tx.payload != NULL);

	net_pkt_append_buffer(mac_port_ctx.tx.pkt, mac_port_ctx.tx.payload);
	
	k_work_init(&mac_port_ctx.tx.tx_work_item, tx_work_handler);

	k_sem_init(&mac_port_ctx.tx.tx_start_sem, 0, 1);

	aCfg.event_handler = handle_radio_event;
	mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_CONFIG_EVENT_HANDLER, &aCfg);

	mac_port_ctx.isAntDivEnabled = false;

	return;
}

enum net_verdict ieee802154_handle_ack(struct net_if *aIfacePtr, struct net_pkt *aPktPtr)
{
	ARG_UNUSED(aIfacePtr);

	LOG_DBG("ieee802154_handle_ack");

	size_t aAckLen = net_pkt_get_len(aPktPtr);

	if (net_pkt_read(aPktPtr, mac_port_ctx.tx.ack_psdu, aAckLen) < 0) {
		LOG_ERR("Failed to read ACK frame.");
		return NET_CONTINUE;
	}

	mac_port_ctx.tx.ack_frame.mPsdu = mac_port_ctx.tx.ack_psdu;
	mac_port_ctx.tx.ack_frame.mLength = aAckLen;
	mac_port_ctx.tx.ack_frame.mInfo.mRxInfo.mLqi = net_pkt_ieee802154_lqi(aPktPtr);
	mac_port_ctx.tx.ack_frame.mInfo.mRxInfo.mRssi = net_pkt_ieee802154_rssi_dbm(aPktPtr);

#if defined(CONFIG_NET_PKT_TIMESTAMP)
	mac_port_ctx.tx.ack_frame.mInfo.mRxInfo.mTimestamp = net_pkt_timestamp_ns(aPktPtr) / NSEC_PER_USEC;
#endif

	return NET_OK;
}

void radio_init(void)
{
	LOG_DBG("LL init is already done by the Zephyr OS, Do Nothing");

	return;
}

void radio_call_back_funcs_init(struct mac_cbk_dispatch_tbl *aCbkDispatchTblPtr)
{
	LOG_DBG("radio_call_back_funcs_init");

	mac_port_ctx.cbk_dispatch_tbl = aCbkDispatchTblPtr;

	return;
}

otError otPlatRadioReceive(otInstance *aInstancePtr, uint8_t aChannel)
{
	LOG_DBG("otPlatRadioReceive on channel %d", aChannel);

	mac_port_ctx.instance = aInstancePtr;

	mac_port_ctx.rx.channel = aChannel;
	mac_port_ctx.api->set_channel(mac_port_ctx.dev, aChannel);
	mac_port_ctx.api->start(mac_port_ctx.dev);

	return OT_ERROR_NONE;
}


otError otPlatRadioTransmit(otInstance *aInstancePtr, otRadioFrame *aFramePtr)
{
	ARG_UNUSED(aInstancePtr);
	
	LOG_DBG("otPlatRadioTransmit");

	if (aFramePtr->mPsdu == NULL) {
		LOG_ERR("Invalid mPsdu pointer in aFramePtr");
		return OT_ERROR_INVALID_ARGS;
	}
	
	mac_port_ctx.instance = aInstancePtr;

	mac_port_ctx.api->set_channel(mac_port_ctx.dev, aFramePtr->mChannel);

	set_max_csma_backoff(aFramePtr->mInfo.mTxInfo.mMaxCsmaBackoffs);
	set_max_frm_retries(aFramePtr->mInfo.mTxInfo.mMaxFrameRetries);

	mac_port_ctx.tx.payload->data = aFramePtr->mPsdu;
	mac_port_ctx.tx.payload->len = aFramePtr->mLength - IEEE802154_FCS_LENGTH;

	net_pkt_set_ieee802154_frame_secured(mac_port_ctx.tx.pkt, aFramePtr->mInfo.mTxInfo.mIsSecurityProcessed);
	net_pkt_set_ieee802154_mac_hdr_rdy(mac_port_ctx.tx.pkt, aFramePtr->mInfo.mTxInfo.mIsHeaderUpdated);

	memset(&mac_port_ctx.tx.ack_frame, 0, sizeof(otRadioFrame));
	
	mac_port_ctx.tx.tx_frame = aFramePtr;
	mac_port_ctx.tx.mode = (aFramePtr->mInfo.mTxInfo.mCsmaCaEnabled)? IEEE802154_TX_MODE_CSMA_CA: IEEE802154_TX_MODE_DIRECT;
	mac_port_ctx.tx.status = OT_ERROR_NONE;
	mac_port_ctx.tx.isTxStarted = false;

	k_sem_reset(&mac_port_ctx.tx.tx_start_sem);
	
	/* Submit the TX work to the system work queue */
	k_work_submit(&mac_port_ctx.tx.tx_work_item);

	/* Wait until Tx started*/
	k_sem_take(&mac_port_ctx.tx.tx_start_sem, K_FOREVER);

	return mac_port_ctx.tx.status;
}

otError otPlatRadioEnergyScan(otInstance *aInstancePtr, uint8_t aScanChannel, uint16_t aScanDuration)
{
	otError aStatus = OT_ERROR_NONE;
	
	LOG_DBG("otPlatRadioEnergyScan on channel %d", aScanChannel);

	mac_port_ctx.instance = aInstancePtr;

	mac_port_ctx.api->set_channel(mac_port_ctx.dev, aScanChannel);

	if (mac_port_ctx.api->ed_scan(mac_port_ctx.dev, aScanDuration, energy_detected) != 0)	{
		LOG_ERR("Failed to perform ED scan");
		aStatus = OT_ERROR_FAILED;
	}

	return aStatus;
}

otError otPlatRadioSleep(otInstance *aInstancePtr)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioSleep");

	mac_port_ctx.api->stop(mac_port_ctx.dev);

	return OT_ERROR_NONE;
}

otError otPlatRadioAddSrcMatchExtEntry(otInstance *aInstancePtr, const otExtAddress *aExtAddressPtr)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioAddSrcMatchExtEntry");

	struct ieee802154_config aConfig = {
		.ack_fpb.enabled = true,
		.ack_fpb.addr = (uint8_t *)aExtAddressPtr->m8,
		.ack_fpb.extended = true
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_CONFIG_ACK_FPB, &aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_CONFIG_ACK_FPB");
		return OT_ERROR_FAILED;
	}

	return OT_ERROR_NONE;
}

otError otPlatRadioClearSrcMatchExtEntry(otInstance *aInstancePtr, const otExtAddress *aExtAddressPtr)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioClearSrcMatchExtEntry");

	struct ieee802154_config aConfig = {
		.ack_fpb.enabled = false,
		.ack_fpb.addr = (uint8_t *)aExtAddressPtr->m8,
		.ack_fpb.extended = true
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_CONFIG_ACK_FPB, &aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_CONFIG_ACK_FPB");
		return OT_ERROR_FAILED;
	}

	return OT_ERROR_NONE;
}

otError otPlatRadioAddSrcMatchShortEntry(otInstance *aInstancePtr, const uint16_t aShortAddress)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioAddSrcMatchShortEntry on address %d", aShortAddress);

	uint8_t shortAddressBuf[SHORT_ADDRESS_SIZE];
	sys_put_le16(aShortAddress, shortAddressBuf);

	struct ieee802154_config aConfig = {
		.ack_fpb.enabled = true,
		.ack_fpb.addr = shortAddressBuf,
		.ack_fpb.extended = false
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_CONFIG_ACK_FPB, &aConfig) != 0){
		LOG_ERR("Failed to configure IEEE802154_CONFIG_ACK_FPB");
		return OT_ERROR_FAILED;
	}

	return OT_ERROR_NONE;
}

otError otPlatRadioClearSrcMatchShortEntry(otInstance *aInstancePtr, const uint16_t aShortAddress)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioClearSrcMatchShortEntry on address %d", aShortAddress);

	uint8_t shortAddressBuf[SHORT_ADDRESS_SIZE];
	sys_put_le16(aShortAddress, shortAddressBuf);

	struct ieee802154_config aConfig = {
		.ack_fpb.enabled = false,
		.ack_fpb.addr = shortAddressBuf,
		.ack_fpb.extended = false
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_CONFIG_ACK_FPB, &aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_CONFIG_ACK_FPB");
		return OT_ERROR_FAILED;
	}

	return OT_ERROR_NONE;
}

void otPlatRadioEnableSrcMatch(otInstance *aInstancePtr, bool aEnable)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioEnableSrcMatch %d", aEnable);

	struct ieee802154_config aConfig = {
		.auto_ack_fpb.enabled = aEnable,
		.auto_ack_fpb.mode = IEEE802154_FPB_ADDR_MATCH_THREAD,
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_CONFIG_AUTO_ACK_FPB, &aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_CONFIG_AUTO_ACK_FPB");
	}

	return;
}

void otPlatRadioSetShortAddress(otInstance *aInstancePtr, uint16_t aShortAddress)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioSetShortAddress %d", aShortAddress);

	struct ieee802154_filter filter = {
		.short_addr = aShortAddress
	};

	if (mac_port_ctx.api->filter(mac_port_ctx.dev, true, IEEE802154_FILTER_TYPE_SHORT_ADDR, &filter) != 0) {
		LOG_ERR("Failed to set IEEE802154_FILTER_TYPE_SHORT_ADDR");
	}

	return;
}

void otPlatRadioSetExtendedAddress(otInstance *aInstancePtr, const otExtAddress *aExtAddressPtr)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioSetExtendedAddress");

	struct ieee802154_filter filter = {
		.ieee_addr = ((otExtAddress *)aExtAddressPtr)->m8
	};	

	if (mac_port_ctx.api->filter(mac_port_ctx.dev, true, IEEE802154_FILTER_TYPE_IEEE_ADDR, &filter) != 0) {
		LOG_ERR("Failed to set IEEE802154_FILTER_TYPE_IEEE_ADDR");
	}

	return;
}

void otPlatRadioSetPromiscuous(otInstance *aInstancePtr, bool aEnable)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioSetPromiscuous %d", aEnable);

	struct ieee802154_config aConfig = {
		.promiscuous = aEnable
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_CONFIG_PROMISCUOUS, &aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_CONFIG_PROMISCUOUS");
	}

	return;
}

void otPlatRadioGetIeeeEui64(otInstance *aInstancePtr, uint8_t *aIeeeEui64Ptr)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("Fetching IEEE EUI-64 address");

	struct ieee802154_stm32wba_attr_value aValue;

	if (mac_port_ctx.api->attr_get(mac_port_ctx.dev, IEEE802154_STM32WBA_ATTR_IEEE_EUI64, (struct ieee802154_attr_value *)&aValue) != 0) {
		LOG_ERR("Failed to get IEEE802154_STM32WBA_ATTR_IEEE_EUI64");
	}

	memcpy(aIeeeEui64Ptr, aValue.eui64, sizeof(aValue.eui64));
	LOG_DBG("IEEE EUI-64 address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		aIeeeEui64Ptr[0], aIeeeEui64Ptr[1], aIeeeEui64Ptr[2], aIeeeEui64Ptr[3],
		aIeeeEui64Ptr[4], aIeeeEui64Ptr[5], aIeeeEui64Ptr[6], aIeeeEui64Ptr[7]);

	return;
}

otError otPlatRadioSetTransmitPower(otInstance *aInstancePtr, int8_t aPower)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioSetTransmitPower %d", aPower);

	if (mac_port_ctx.api->set_txpower(mac_port_ctx.dev, aPower) != 0) {
		LOG_ERR("Failed to set transmit power");
		return OT_ERROR_FAILED;
	}

	return OT_ERROR_NONE;
}

otError otPlatRadioGetTransmitPower(otInstance *aInstancePtr, int8_t *aPower)
{
	ARG_UNUSED(aInstancePtr);
	
	struct ieee802154_stm32wba_attr_value aValue = {
		.tx_power = aPower
	};

	if (mac_port_ctx.api->attr_get(mac_port_ctx.dev, IEEE802154_STM32WBA_ATTR_TX_POWER, (struct ieee802154_attr_value *)&aValue) != 0) {
		LOG_ERR("Failed to get IEEE802154_STM32WBA_ATTR_TX_POWER");
		return OT_ERROR_FAILED;
	}

	LOG_DBG("otPlatRadioGetTransmitPower %d", *aPower);

	return OT_ERROR_NONE;
}

void setPANcoordinator(uint8_t aEnable)
{	
	LOG_DBG("Setting PAN Coordinator to %d", aEnable);

	struct ieee802154_config aConfig = {
		.pan_coordinator = aEnable
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_CONFIG_PAN_COORDINATOR, &aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_CONFIG_PAN_COORDINATOR");
	}

	return;
}

void otPlatRadioSetPanId(otInstance *aInstancePtr, uint16_t aPanId)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioSetPanId %d", aPanId);

	struct ieee802154_filter filter = {
		.pan_id = aPanId
	};

	if (mac_port_ctx.api->filter(mac_port_ctx.dev, true, IEEE802154_FILTER_TYPE_PAN_ID, &filter) != 0) {
		LOG_ERR("Failed to set IEEE802154_FILTER_TYPE_PAN_ID");
	}

	return;
}

uint32_t radio_reset(void)
{
	uint32_t aStatus = 0;

	LOG_DBG("radio_reset");

	struct ieee802154_stm32wba_config aConfig = {
		.radio_reset = true
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_STM32WBA_CONFIG_RADIO_RESET, (struct ieee802154_config *)&aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_STM32WBA_CONFIG_RADIO_RESET");
		aStatus = 1;
	}

	return aStatus;
}

otRadioFrame * otPlatRadioGetTransmitBuffer(otInstance *aInstancePtr)
{
	ARG_UNUSED(aInstancePtr);

	otRadioFrame * TransmitFrame = NULL;

	TransmitFrame = allocate_radio_frame();

	return TransmitFrame;
}

otError otPlatRadioSetCcaEnergyDetectThreshold(otInstance *aInstancePtr, int8_t aThreshold)
{
	ARG_UNUSED(aInstancePtr);

	LOG_DBG("otPlatRadioSetCcaEnergyDetectThreshold %d", aThreshold);

	struct ieee802154_stm32wba_config aConfig = {
		.cca_thr = aThreshold
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_STM32WBA_CONFIG_CCA_THRESHOLD, (struct ieee802154_config *)&aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_STM32WBA_CONFIG_CCA_THRESHOLD");
		return OT_ERROR_FAILED;
	}

	return OT_ERROR_NONE;
}

otError otPlatRadioGetCcaEnergyDetectThreshold(otInstance *aInstancePtr, int8_t *aThresholdPtr)
{
	ARG_UNUSED(aInstancePtr);

	struct ieee802154_stm32wba_attr_value aValue = {
		.cca_thr = aThresholdPtr
	};

	if (mac_port_ctx.api->attr_get(mac_port_ctx.dev, IEEE802154_STM32WBA_ATTR_CCA_THRESHOLD, (struct ieee802154_attr_value *)&aValue) != 0) {
		LOG_ERR("Failed to get IEEE802154_STM32WBA_ATTR_CCA_THRESHOLD");
		return OT_ERROR_FAILED;
	}

	LOG_DBG("CCA Energy Detect Threshold=%d", *aThresholdPtr);

	return OT_ERROR_NONE;
}

void setContRecp(uint8_t aEnable)
{
	LOG_DBG("setContRecp %d", aEnable);

	struct ieee802154_stm32wba_config aConfig = {
		.en_cont_rec = (bool)aEnable
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_STM32WBA_CONFIG_CONTINUOUS_RECEPTION, (struct ieee802154_config *)&aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_STM32WBA_CONFIG_CONTINUOUS_RECEPTION");
	}

	return;
}

void set_max_frm_retries(uint8_t aValue)
{
	LOG_DBG("set_max_frm_retries %d", aValue);

	struct ieee802154_stm32wba_config aConfig = {
		.max_frm_retries = aValue
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_STM32WBA_CONFIG_MAX_FRAME_RETRIES, (struct ieee802154_config *)&aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_STM32WBA_CONFIG_MAX_FRAME_RETRIES");
	}

	return;
}

void set_min_csma_be(uint8_t aValue)
{
	LOG_DBG("set_min_csma_be %d", aValue);

	struct ieee802154_stm32wba_config aConfig = {
		.min_csma_be = aValue
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_STM32WBA_CONFIG_MIN_CSMA_BE, (struct ieee802154_config *)&aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_STM32WBA_CONFIG_MIN_CSMA_BE");
	}

	return;
}

void set_max_csma_be(uint8_t aValue)
{
	LOG_DBG("set_max_csma_be %d", aValue);

	struct ieee802154_stm32wba_config aConfig = {
		.max_csma_be = aValue
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_STM32WBA_CONFIG_MAX_CSMA_BE, (struct ieee802154_config *)&aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_STM32WBA_CONFIG_MAX_CSMA_BE");
	}

	return;
}

void set_max_csma_backoff(uint8_t aValue)
{
	LOG_DBG("set_max_csma_backoff %d", aValue);

	struct ieee802154_stm32wba_config aConfig = {
		.max_csma_backoff = aValue
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_STM32WBA_CONFIG_MAX_CSMA_BACKOFF, (struct ieee802154_config *)&aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_STM32WBA_CONFIG_MAX_CSMA_BACKOFF");
	}

	return;
}

void set_max_full_csma_frm_retries(uint8_t aValue)
{
	LOG_DBG("set_max_full_csma_frm_retries %d", aValue);

	struct ieee802154_stm32wba_config aConfig = {
		.max_csma_frm_retries = aValue
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_STM32WBA_CONFIG_MAX_CSMA_FRAME_RETRIES, (struct ieee802154_config *)&aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_STM32WBA_CONFIG_MAX_CSMA_FRAME_RETRIES");
	}

	return;
}

void radio_set_implicitbroadcast(uint8_t aImplicitBroadcast)
{
	LOG_DBG("radio_set_implicitbroadcast %d", aImplicitBroadcast);

	struct ieee802154_stm32wba_config aConfig = {
		.impl_brdcast = (bool)aImplicitBroadcast
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_STM32WBA_CONFIG_IMPLICIT_BROADCAST, (struct ieee802154_config *)&aConfig) != 0) {
		LOG_ERR("Failed to configure IEEE802154_STM32WBA_CONFIG_IMPLICIT_BROADCAST");
	}

	return;
}

void radio_set_csma_en(uint8_t csma_en){
	ARG_UNUSED(csma_en);
	LOG_DBG("radio_set_csma_en %d", csma_en);
	return;
}

uint8_t radio_get_csma_en(void){
	LOG_DBG("radio_get_csma_en = %d", true);
	return true;
}

otError radio_set_ant_div_enable(otInstance *aInstance, uint8_t aEnable)
{
	ARG_UNUSED(aInstance);
	LOG_DBG("radio_set_ant_div_enable %d", aEnable);
	
	struct ieee802154_stm32wba_config aConfig = {
		.ant_div = aEnable
	};

	if (mac_port_ctx.api->configure(mac_port_ctx.dev, IEEE802154_STM32WBA_CONFIG_ANTENNA_DIV, (struct ieee802154_config *)&aConfig) != 0) {
		if(mac_port_ctx.isAntDivEnabled != aEnable) {
			LOG_ERR("Failed to configure IEEE802154_STM32WBA_CONFIG_ANTENNA_DIV");
		}
	}
	else{
		LOG_DBG("Antenna diversity enabled %d", aEnable);
		mac_port_ctx.isAntDivEnabled = aEnable;
	}

	return OT_ERROR_NONE;
}

uint32_t mac_gen_rnd_num(uint8_t *aPtrRnd, uint16_t aLen, uint8_t aCheckContRx)
{
	ARG_UNUSED(aCheckContRx);
	uint32_t aStatus = SUCCESS;

	if (aLen != 1) {
		LOG_ERR("Invalid length %d. This function only supports Len = 1", aLen);
		aStatus = GENERAL_ERROR_STATUS;
		goto FuncExit;
	}
	
	struct ieee802154_stm32wba_attr_value aValue = {
		.rand_num = aPtrRnd
	};

	if (mac_port_ctx.api->attr_get(mac_port_ctx.dev, IEEE802154_STM32WBA_ATTR_RAND_NUM, (struct ieee802154_attr_value *)&aValue) != 0) {
		LOG_ERR("Failed to get IEEE802154_STM32WBA_ATTR_RAND_NUM");
		return GENERAL_ERROR_STATUS;
	}

	LOG_DBG("mac_gen_rnd_num %d", *aPtrRnd);

FuncExit:
	return aStatus;
}

NET_L2_INIT(CUSTOM_IEEE802154_L2, ieee802154_stm_recv, ieee802154_stm_send, ieee802154_stm_enable, ieee802154_stm_flags);
