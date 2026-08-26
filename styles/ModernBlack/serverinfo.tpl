<!--
TeamSpeak 3 Server Infoframe Template - Modern Black by Coretify Studio
-->
<div class="InfoFrame" title="<table><tr><td><b><nobr>%%TR_SERVER_CLIENTS_ONLINE%%:&nbsp;</nobr></b></td><td><nobr>%%SERVER_CLIENTS_ONLINE%% / %%SERVER_MAXCLIENTS%%</nobr></td></tr><tr><td><b><nobr>%%TR_SERVER_CLIENT_CONNECTIONS%%:&nbsp;</nobr></b></td><td><nobr>%%SERVER_CLIENT_CONNECTIONS%%</nobr></td></tr><tr><td colspan=2><hr /></td></tr><tr><td><b><nobr>%%TR_SERVER_VERSION%%:&nbsp;</nobr></b></td><td><nobr>%%SERVER_VERSION%%</nobr></td></tr></table>">

  <div class="InfoFrame_Title" dir="LTR">
    %%SERVER_ICON_SCALED%%
    &nbsp;
    <a href="channelid://0" class="TextMessage_ServerLink">%%SERVER_NAME%%</a>
  </div>

  <hr />

  <table class="InfoFrame_Table">
    <tr><td class="Label">%%TR_SERVER_NICKNAMES%%:</td><td>%%?SERVER_NICKNAMES%%</td></tr>
	<tr>
      <td class="Label">%%TR_SERVER_ADDRESS%%:</td>
      <td>
	    %%SERVER_ADDRESS%%
	    :%%?SERVER_PORT%%
	  </td>
    </tr>
    <tr>
      <td class="Bottom Label">%%TR_SERVER_VERSION%%:</td>
      <td class="Bottom">%%SERVER_VERSION_SHORT%% %%SERVER_PLATFORM%%</td>
    </tr>
    <tr>
      <td class="Label">%%TR_SERVER_LICENSE%%:</td>
      <td>%%SERVER_LICENSE%%</td>
    </tr>
    <tr>
      <td class="Label">%%TR_SERVER_UPTIME%%:</td>
      <td>%%SERVER_UPTIME%%</td>
    </tr>
  </table>

  <br />

  <table class="InfoFrame_Table">
    <tr>
      <td class="Label">%%TR_SERVER_CLIENTS_ONLINE%%:</td>
      <td>%%SERVER_CLIENTS_ONLINE%% / %%SERVER_MAXCLIENTS%% %%?SERVER_NO_RESERVED_SLOTS%%</td>
      <td>%%SERVER_CLIENTS_ONLINE%% / %%SERVER_MAXCLIENTS%% (<span class="Important">-%%?SERVER_RESERVED_SLOTS%% reserved</span>)</td>
    </tr>
    <tr>
      <td class="Label">%%TR_SERVER_CHANNELS_ONLINE%%:</td>
      <td>%%SERVER_CHANNELS_ONLINE%%</td>
    </tr>
    %%?PLUGIN_INFO_DATA%%
  </table>

  <br />

  <table class="InfoFrame_Table">
    <tr><td>
      %%SERVER_REFRESH_ICON%%
      &nbsp;
      <a class="Inactive" href="%%?SERVER_REFRESH_INACTIVE%%">%%?TR_SERVER_REFRESH_INACTIVE%%</a>
      <a class="Active" href="%%?SERVER_REFRESH_ACTIVE%%">%%?TR_SERVER_REFRESH_ACTIVE%%</a>
    </td></tr>
  </table>

  <div style="margin-top: 25px; padding-top: 8px; border-top: 1px solid #181C26; text-align: center; color: #555A64; font-size: 8pt; font-family: 'Segoe UI', sans-serif;">
    ✦ This UI was designed by <b style="color: #3B82F6;">Coretify Studio</b> ✦
  </div>

</div>
