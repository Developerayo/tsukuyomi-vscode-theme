<?php
require_once(__DIR__ . '/includes/autoload.php');

if(isset($_GET['a']) && isset($action[$_GET['a']])) {
	$page_name = $action[$_GET['a']];
} else {
	$page_name = 'welcome';
}

if(!isAjax()) {
	$TMPL['token_id'] = generateToken();
}

// Extra class for the content [main and sidebar]
$TMPL['content_class'] = ' content-'.$page_name;

require_once("./sources/{$page_name}.php");

$TMPL['volume'] = $settings['volume'];
$TMPL['supplied_formats'] = $settings['trackformat'];
$TMPL['site_title'] = $settings['title'];
$TMPL['site_url'] = $CONF['url'];

if(isAjax()) {
	echo json_encode(array('content' => PageMain(), 'title' => $TMPL['title']));
	mysqli_close($db);
	return;
}
$TMPL['content'] = PageMain();

if(!empty($user['username'])) {
	$TMPL['menu'] = menu($user);
	$TMPL['menu_buttons'] = menuButtons($user);
	$TMPL['url_menu'] = permalink($CONF['url'].'/index.php?a=stream');
} else {
	$TMPL['menu'] = menu(false);
	$TMPL['menu_buttons'] = menuButtons(false);
	$TMPL['url_menu'] = permalink($CONF['url'].'/index.php?a=welcome');
}

if($settings['captcha']) {
	// Captcha
	$TMPL['captcha'] = '<div class="modal-captcha"><input type="text" name="captcha" placeholder="'.$LNG['captcha'].'"></div>
	<span class="register-captcha" id="captcha-register"><img src="'.$CONF['url'].'/includes/captcha.php" /></span>';
}
if($settings['fbapp']) {
	// Generate a session to prevent CSFR
	$_SESSION['state'] = md5(uniqid(rand(), TRUE));
	
	// Facebook Login Url
	$TMPL['fblogin'] = '<div class="modal-btn modal-btn-facebook"><a href="https://www.facebook.com/dialog/oauth?client_id='.$settings['fbappid'].'&redirect_uri='.urlencode($CONF['url'].'/requests/connect.php?facebook=true').'&state='.$_SESSION['state'].'&scope=public_profile,email" class="facebook-button">Facebook</a></div>';
}

$TMPL['url'] = $CONF['url'];
$TMPL['year'] = date('Y');
$TMPL['info_urls'] = info_urls();
$TMPL['powered_by'] = 'Powered by <a href="'.$url.'" target="_blank">'.$name.'</a>.';
$TMPL['language'] = getLanguage($CONF['url'], null, 1);
$TMPL['tracking_code'] = $settings['tracking_code'];
$TMPL['page_url'] = permalink($CONF['url'].'/index.php?a=page&b=');
$TMPL['notifications_url'] = permalink($CONF['url'].'/index.php?a=notifications');
$TMPL['notifications_chats_url'] = permalink($CONF['url'].'/index.php?a=notifications&filter=chats');
$TMPL['settings_notifications_url'] = permalink($CONF['url'].'/index.php?a=settings&b=notifications');
$TMPL['recover_url'] = permalink($CONF['url'].'/index.php?a=recover');
$TMPL['search_filter'] = permalink($CONF['url'].'/index.php?a=search&filter=tracks&q=');
$TMPL['explore_filter'] = permalink($CONF['url'].'/index.php?a=explore&filter=');
$TMPL['agreement'] = sprintf($LNG['register_agreement'], permalink($settings['tos_url']), permalink($settings['privacy_url']));
$TMPL['cookie_banner'] = isset($_COOKIE['cookie_law']) == false && $settings['cookie_url'] ? cookie_law() : null;

$skin = new skin('wrapper');

echo $skin->make();

mysqli_close($db);
?>