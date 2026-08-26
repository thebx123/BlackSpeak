<!--
TeamSpeak 3 Client Infoframe Template - Modern Black by Coretify Studio
-->
<div class="InfoFrame" title="<table><tr><td><b><nobr>%%TR_CLIENT_CREATED%%:&nbsp;</nobr></b></td><td><nobr>%%CLIENT_CREATED%%</nobr></td></tr><tr><td><b><nobr>%%TR_CLIENT_LASTCONNECTED%%:&nbsp;</nobr></b></td><td><nobr>%%CLIENT_LASTCONNECTED%%</nobr></td></tr><tr><td><b><nobr>%%TR_CLIENT_TOTALCONNECTIONS%%:&nbsp;</nobr></b></td><td><nobr>%%CLIENT_TOTALCONNECTIONS%%</nobr></td></tr><tr><td colspan=2><hr /></td></tr><tr><td><b><nobr>%%TR_CLIENT_VERSION%%:&nbsp;</nobr></b></td><td><nobr>%%CLIENT_VERSION%% %%CLIENT_VERSION_STATE%%</nobr></td></tr></table>">

  <div class="InfoFrame_Title" dir="LTR">
    %%?CLIENT_COUNTRY_IMAGE_SCALED%%
    <a href="client://%%CLIENT_ID%%/%%CLIENT_UNIQUE_ID%%~%%CLIENT_NAME_PERCENT_ENCODED%%" class="TextMessage_UserLink">
    &nbsp;%%CLIENT_NAME%%&nbsp;</a>
    <span class="InfoFrame_CustomNick" title="Custom Nickname">[%%?CLIENT_CUSTOM_NICK_NAME%%]</span>
  </div>

  <hr />

  <table class="InfoFrame_Table">
    <tr>
      <td class="Bottom Label">%%TR_CLIENT_VERSION%%:</td>
	  <td class="Bottom">%%CLIENT_VERSION_LONG%%</td>
    </tr>
    <tr><td class="Label">%%TR_CLIENT_CONNECTED_SINCE%%:</td><td>%%CLIENT_CONNECTED_SINCE%%</td></tr>
    <tr><td class="Label">%%?TR_CLIENT_DESCRIPTION%%:</td><td>%%?CLIENT_DESCRIPTION%%</td></tr>
    <tr><td class="Label">%%?TR_CLIENT_VOLUME_MODIFIER%%:</td><td class="Important">%%?CLIENT_VOLUME_MODIFIER%% dB</td></tr>
  </table>

  <br /><table class="InfoFrame_Table">%%?PLUGIN_INFO_DATA%%</table>

  %%??CLIENT_FLAG_BADGES%%<table class="InfoFrame_Table">
  %%??CLIENT_FLAG_BADGES%%  <tr>
  %%??CLIENT_FLAG_BADGES%%    <td colspan="%%CLIENT_FLAG_BADGES%%" class="Label Space-Top">%%TR_CLIENT_BADGE_SHOWCASE%%:</td>
  %%??CLIENT_FLAG_BADGES%%  </tr>
  %%??CLIENT_FLAG_BADGES%%  <tr>
  %%??CLIENT_FLAG_BADGES%%    <td class="Badge" title="<b>%%CLIENT_BADGE_NAME%%</b><br />%%CLIENT_BADGE_DESCRIPTION%%">%%CLIENT_BADGE_ICON_LARGE%%</td>
  %%??CLIENT_FLAG_BADGES%%  </tr>
  %%??CLIENT_FLAG_BADGES%%</table>

  <table class="InfoFrame_Table Space-Bot">
    <tr><td class="Label Space-Top">%%TR_CLIENT_SERVER_GROUPS%%:</td></tr>
    <tr><td class="List">
	  <table>
		<tr valign="middle"><td>%%CLIENT_SERVER_GROUP_ICON%%</td><td>%%CLIENT_SERVER_GROUP_NAME%%</td></tr>
	  </table>
    </td></tr>
    <tr><td class="Label Space-Top">%%TR_CLIENT_CHANNEL_GROUP%%:</td></tr>
    <tr><td class="List">
	  <table>
		<tr valign="middle"><td>%%CLIENT_CHANNEL_GROUP_ICON%%</td><td>%%CLIENT_CHANNEL_GROUP_NAME%%</td></tr>
	  </table>
    </td></tr>
    <tr><td class="Important"><br />*** %%?TR_CLIENT_TALK_REQUEST_TIME%%</td></tr>
    <tr><td class="Important">&nbsp;&nbsp;&nbsp;&nbsp;(%%?CLIENT_TALK_REQUEST_MSG%%)</td></tr>
  </table>

  <div style="margin-top: 25px; padding-top: 8px; border-top: 1px solid #181C26; text-align: center; color: #555A64; font-size: 8pt; font-family: 'Segoe UI', sans-serif;">
    ✦ This UI was designed by <b style="color: #3B82F6;">Coretify Studio</b> ✦
  </div>

</div>
