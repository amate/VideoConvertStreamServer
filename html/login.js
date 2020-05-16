


if (location.search.match(/\?password=/) != null) {
    $('#WrongPassword').show();
}

let matchResult = location.search.match(/\?autoLoginPassword=(.+)/);
if (matchResult != null) {
    let autoLoginPassword = matchResult[1];
    document.cookie = "password=" + autoLoginPassword + "; samesite=strict";
    location.replace("/");
}

$('#SubmitButton').on('click', function() {
    let password = $('#InputPassword1').val();
    document.cookie = "password=" + password + "; samesite=strict";

    let queryPath = "";
    let matchResult = location.search.match(/path=(.*)/);
    if (matchResult != null) {
        queryPath = '&path=' + matchResult[1];
    }

    let jumpToURL = location.pathname + "?password=" + password + queryPath;
    //alert(jumpToURL);
    location.replace(jumpToURL);
    return false;
});
