VERS_1.0 {
  global:
    Agent_OnLoad;
    Agent_OnUnload;

    Java_com_google_devtools_cdbg_debuglets_java_AgentLogger_info;
    Java_com_google_devtools_cdbg_debuglets_java_AgentLogger_warn;
    Java_com_google_devtools_cdbg_debuglets_java_AgentLogger_severe;

    Java_com_google_devtools_cdbg_debuglets_java_GcpEnvironment_getAgentFlag;

    Java_com_google_devtools_cdbg_debuglets_java_Statistician_getStatistics;

    google_find_phdr;

  local:
    *;
};


