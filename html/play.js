
// ファイル名設定
let pathArray = decodeURI(location.pathname).replace(/<hash>/g, "#").split("/");
let fileName = pathArray[pathArray.length - 1];
$('#FileName').text(fileName);

// パンくずリスト設定
pathArray.shift();
pathArray.shift();
pathArray.pop();
pathArray.unshift("root");
let parentPath = "/";
for (let i = 0; i < pathArray.length; ++i) {
    let name = pathArray[i];
    parentPath += name.replace(/#/g, "<hash>") + "/";
    let anchroTag = $('<a>', {
        href: parentPath + location.search
    }).text(name);
    let liTag = $('<li class="breadcrumb-item"></li>').append(anchroTag);
    $('#PankuzuList').append(liTag);
}

// キャッシュチェック
let filePath = decodeURI(location.pathname).replace(/<hash>/g, "#");
let queryPlay = {
    path: filePath
};
$.getJSON("/CacheCheckAPI", queryPlay)
.done(function(json){
    if (json.CacheExist) {
        $('#FileName').prepend('<span class="badge badge-success">cache</span> ');
    }
})
.fail(function(jqXHR, textStatus, errorThrown) {
    console.error("getJSON fail: " + textStatus);
});

var QueryPrepareHLS = function()
{
    let filePath = decodeURI(location.pathname).replace(/<hash>/g, "#");

    let queryPlay = {
        path: filePath
    };

    $.getJSON("/PrepareHLSAPI", queryPlay)
    .done(function(json){
        var video = document.getElementById('video');
        if (json.DirectPlay) {
            $('#FileName').prepend('<span class="badge badge-info">direct</span> ');

            video.src = json.playListURL;
            video.play();
            return;
        }

        let playListURL = json.playListURL;
        console.log('playListURL: ' + playListURL);

        var videoSrc = playListURL;//'/stream/test.m3u8';
        if (Hls.isSupported()) {
            let hlsConfig = Hls.DefaultConfig;
            hlsConfig.startPosition = 0;

            var hls = new Hls(hlsConfig);
            hls.loadSource(videoSrc);
            hls.attachMedia(video);
            hls.on(Hls.Events.MANIFEST_PARSED, function() {
             video.play();
            });
        }
        // hls.js is not supported on platforms that do not have Media Source
        // Extensions (MSE) enabled.
        //
        // When the browser has built-in HLS support (check using `canPlayType`),
        // we can provide an HLS manifest (i.e. .m3u8 URL) directly to the video
        // element through the `src` property. This is using the built-in support
        // of the plain video element, without using hls.js.
        //
        // Note: it would be more normal to wait on the 'canplay' event below however
        // on Safari (where you are most likely to find built-in HLS support) the
        // video.src URL must be on the user-driven white-list before a 'canplay'
        // event will be emitted; the last video event that can be reliably
        // listened-for when the URL is not on the white-list is 'loadedmetadata'.
        else if (video.canPlayType('application/vnd.apple.mpegurl')) {
            video.src = videoSrc;
            video.addEventListener('loadedmetadata', function() {
            video.play();
            });
        }
        //console.log(json);
    })
    .fail(function(jqXHR, textStatus, errorThrown) {
        console.error("getJSON fail: " + textStatus);
    });

};

$('#PlayButton').on('click', function() {
    $('#PlayButton').hide();
    QueryPrepareHLS();
});
