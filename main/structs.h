struct wifi_ieee80211_h {
    uint16_t frame_ctrl;
    uint16_t duration;
	uint8_t addr1[6];
	uint8_t addr2[6];
    uint8_t addr3[6];
	uint16_t seq_ctrl;
    uint8_t addr4[6];
}__attribute__((packed));