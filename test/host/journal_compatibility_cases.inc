// Production journal + independent verified Beta9 decoder; no device access.
static bool journal_compatibility_case(const char *scenario)
{
    if (!strcmp(scenario,"journal-compat-legacy")) {
        assert(beta9_accepts(legacy_journal_v1,sizeof(legacy_journal_v1),577.25f,1137));
        assert(nvs_set_blob(1,"5fw_operating",legacy_journal_v1,sizeof(legacy_journal_v1))==ESP_OK && nvs_commit(1)==ESP_OK);
        unsigned before=writes;
        assert(nvs_config_init()==ESP_OK && writes==before);
        assert(request(2,"{\"type\":\"tuning\",\"frequencyMHz\":577.25,\"coreVoltageMv\":1137}").code==200);
        Entry *entry=journal();
        assert(entry->size==sizeof(legacy_journal_v1) && !memcmp(entry->bytes,legacy_journal_v1,sizeof(legacy_journal_v1)));
        assert_legacy_journal(577.25f,1137);
        assert(request(2,"{\"type\":\"tuning\",\"frequencyMHz\":525.25,\"coreVoltageMv\":1151}").code==200);
        assert_legacy_journal(525.25f,1151);
        NvsConfigMutation route={.key=NVS_CONFIG_STRATUM_URL,.type=TYPE_STR,.value.str="changed.fixture.invalid"};
        assert(nvs_config_apply_atomic(&route,1)==ESP_OK);
        assert_legacy_journal(525.25f,1151);
        before=writes;assert(nvs_config_init()==ESP_OK && writes==before);
        assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==525.25f && nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE)==1151);
        assert_string(NVS_CONFIG_STRATUM_URL,"changed.fixture.invalid");
        assert_auth(false,false);
        assert(!find(1,"sv2reqauth") && !find(1,"fbsv2reqauth") && !erases);
        return true;
    }
    if (!strncmp(scenario,"journal-compat-raw-",19)) {
        unsigned primary,fallback;
        assert(sscanf(scenario,"journal-compat-raw-%1u%1u",&primary,&fallback)==2 && primary<=1 && fallback<=1);
        put(1,"sv2reqauth",3,NULL,primary);put(1,"fbsv2reqauth",3,NULL,fallback);
        unsigned before=writes;assert(nvs_config_init()==ESP_OK && writes==before);
        assert_auth(primary,fallback);
        assert(request(2,"{\"type\":\"tuning\",\"frequencyMHz\":525.25,\"coreVoltageMv\":1151}").code==200);
        assert_legacy_journal(525.25f,1151);assert_raw_auth(primary,fallback);
        set_auth(primary,fallback);assert_legacy_journal(525.25f,1151);
        before=writes;assert(nvs_config_init()==ESP_OK && writes==before);assert_auth(primary,fallback);
        set_auth(!primary,fallback);assert_extended_journal();assert_raw_auth(primary,fallback);
        before=writes;assert(nvs_config_init()==ESP_OK && writes==before);assert_auth(!primary,fallback);
        // The baseline must still be the raw keys, not the replayed auth values.
        assert(request(2,"{\"type\":\"tuning\",\"frequencyMHz\":525.25,\"coreVoltageMv\":1151}").code==200);
        assert_extended_journal();assert_auth(!primary,fallback);
        set_auth(primary,!fallback);assert_extended_journal();assert_raw_auth(primary,fallback);
        set_auth(primary,fallback);
        // Reverting to 28 must immediately fix cache, before any reboot.
        assert_legacy_journal(525.25f,1151);assert_auth(primary,fallback);assert_raw_auth(primary,fallback);
        before=writes;assert(nvs_config_init()==ESP_OK && writes==before);assert_auth(primary,fallback);
        assert_raw_auth(primary,fallback);assert(!erases);
        return true;
    }
    if (!strcmp(scenario,"journal-compat-rollback")) {
        assert(request(2,"{\"type\":\"tuning\",\"frequencyMHz\":625.125,\"coreVoltageMv\":1173}").code==200);
        assert_legacy_journal(625.125f,1173);
        size_t old_size=journal()->size;
        unsigned char *old=malloc(old_size);assert(old);memcpy(old,journal()->bytes,old_size);
        NvsConfigMutation change={.key=NVS_CONFIG_SV2_REQUIRE_AUTH,.type=TYPE_BOOL,.value.b=true};
        fail_set=true;assert(nvs_config_apply_atomic(&change,1)!=ESP_OK);fail_set=false;
        assert_auth(false,false);assert(journal()->size==old_size && !memcmp(journal()->bytes,old,old_size));
        // Model an uncertain commit that persisted the new 30-entry blob, then
        // returned failure. The inverse snapshot must independently select 28.
        unsigned before_commits=commits;
        fail_commits_remaining=1;persist_before_commit_failure=true;
        assert(nvs_config_apply_atomic(&change,1)!=ESP_OK);
        persist_before_commit_failure=false;
        assert(commits==before_commits+2 && !fail_commits_remaining);
        assert_auth(false,false);assert_legacy_journal(625.125f,1173);
        assert(journal()->size==old_size && !memcmp(journal()->bytes,old,old_size));free(old);
        unsigned before=writes;assert(nvs_config_init()==ESP_OK && writes==before);assert_auth(false,false);
        set_auth(true,false);assert_extended_journal();
        old_size=journal()->size;old=malloc(old_size);assert(old);memcpy(old,journal()->bytes,old_size);
        // Reverse direction: a failed attempted downgrade to 28 must restore
        // the 30-entry before state, including its changed authentication.
        change.value.b=false;before_commits=commits;
        fail_commits_remaining=1;persist_before_commit_failure=true;
        assert(nvs_config_apply_atomic(&change,1)!=ESP_OK);
        persist_before_commit_failure=false;
        assert(commits==before_commits+2 && !fail_commits_remaining);
        assert_auth(true,false);assert_extended_journal();
        assert(journal()->size==old_size && !memcmp(journal()->bytes,old,old_size));free(old);
        before=writes;assert(nvs_config_init()==ESP_OK && writes==before);assert_auth(true,false);
        set_auth(false,false);assert_legacy_journal(625.125f,1173);assert_auth(false,false);
        before=writes;assert(nvs_config_init()==ESP_OK && writes==before);assert_auth(false,false);
        assert(!find(1,"sv2reqauth") && !find(1,"fbsv2reqauth") && !erases);
        return true;
    }
    if (!strcmp(scenario,"journal-compat-queued-auth")) {
        assert(!find(1,"5fw_operating") && !find(1,"sv2reqauth"));
        nvs_config_set_bool(NVS_CONFIG_SV2_REQUIRE_AUTH,true);drain_auth_write();
        assert_auth(true,false);assert_extended_journal();
        assert(!find(1,"sv2reqauth") && !find(1,"fbsv2reqauth"));
        unsigned before=writes;assert(nvs_config_init()==ESP_OK && writes==before);assert_auth(true,false);
        nvs_config_set_bool(NVS_CONFIG_SV2_REQUIRE_AUTH,false);drain_auth_write();
        assert_auth(false,false);assert_legacy_journal(625.125f,1173);
        before=writes;assert(nvs_config_init()==ESP_OK && writes==before);assert_auth(false,false);
        assert(!find(1,"sv2reqauth") && !find(1,"fbsv2reqauth") && !erases);
        return true;
    }
    if (!strcmp(scenario,"journal-compat-queued-tuning")) {
        put(1,"sv2reqauth",3,NULL,1);put(1,"fbsv2reqauth",3,NULL,0);
        unsigned before=writes;assert(nvs_config_init()==ESP_OK && writes==before);
        assert(!find(1,"5fw_operating"));assert_auth(true,false);
        fail_set=true;
        nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY,525.25f);drain_auth_write();
        fail_set=false;
        assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==625.125f && !find(1,"5fw_operating"));
        fail_commits_remaining=1;persist_before_commit_failure=true;
        nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY,525.25f);drain_auth_write();
        persist_before_commit_failure=false;
        assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==625.125f);
        assert_legacy_journal(625.125f,1173);assert_auth(true,false);assert_raw_auth(true,false);
        nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY,525.25f);drain_auth_write();
        nvs_config_set_u16(NVS_CONFIG_ASIC_VOLTAGE,1151);drain_auth_write();
        assert_legacy_journal(525.25f,1151);assert_auth(true,false);assert_raw_auth(true,false);
        // All queued operating writes use the journal, never a racy raw-key
        // fallback even before a profile has activated an authoritative blob.
        assert(find(1,"asicfrequency_f") && !strcmp((char *)find(1,"asicfrequency_f")->bytes,"625.125"));
        assert(find(1,"asicvoltage")->number==1173);
        before=writes;assert(nvs_config_init()==ESP_OK && writes==before);
        assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==525.25f && nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE)==1151);
        assert_auth(true,false);assert_raw_auth(true,false);assert(!erases);
        return true;
    }
    if (!strcmp(scenario,"journal-compat-queued-failure")) {
        fail_set=true;
        nvs_config_set_bool(NVS_CONFIG_SV2_REQUIRE_AUTH,true);drain_auth_write();
        fail_set=false;assert_auth(false,false);assert(!find(1,"5fw_operating") && !find(1,"sv2reqauth"));
        unsigned before=writes;assert(nvs_config_init()==ESP_OK && writes==before);assert_auth(false,false);
        fail_commits_remaining=1;persist_before_commit_failure=true;
        nvs_config_set_bool(NVS_CONFIG_SV2_REQUIRE_AUTH,true);drain_auth_write();
        persist_before_commit_failure=false;
        assert_auth(false,false);assert_legacy_journal(625.125f,1173);
        before=writes;assert(nvs_config_init()==ESP_OK && writes==before);assert_auth(false,false);
        nvs_config_set_bool(NVS_CONFIG_SV2_REQUIRE_AUTH,true);drain_auth_write();
        assert_auth(true,false);assert_extended_journal();
        before=writes;assert(nvs_config_init()==ESP_OK && writes==before);assert_auth(true,false);
        assert(!find(1,"sv2reqauth") && !find(1,"fbsv2reqauth") && !erases);
        return true;
    }
    return false;
}
