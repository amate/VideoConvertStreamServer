
// パンくずリスト設定
$(document).ready(function(){
    let pathArray = decodeURI(location.pathname).replace(/<hash>/g, "#").split("/");
    let parentPath = "/";
    for (let i = 1; i < pathArray.length - 1; ++i) {
        let name = pathArray[i];
        if (i == pathArray.length - 2) {
            $('#PankuzuList').append(
                $('<li class="breadcrumb-item active" aria-current="page"></li>').text(name));
        } else {
            parentPath += name.replace(/#/g, "<hash>") + "/";
            let anchroTag = $('<a>', {
                href: parentPath + location.search
            }).text(name);
            let liTag = $('<li class="breadcrumb-item"></li>').append(anchroTag);
            $('#PankuzuList').append(liTag);
        }
    }
});

// ソート設定
let sort;
let order;

const kSortOrderMap = {
    "?sort=name&order=asc"  : "Name (A - Z)",
    "?sort=name&order=desc" : "Name (Z - A)",
    "?sort=date&order=asc"  : "Date (New - Old)",
    "?sort=date&order=desc" : "Date (Old - New)",
}
if (location.search.length > 0) {
    $('#DropDownSort').text(kSortOrderMap[location.search]);
} else {
    $('#DropDownSort').text("Name (A - Z)");
}

for (let [key, value] of Object.entries(kSortOrderMap)) {
    let menuItem = $('<button class="dropdown-item" type="button">' + value +'</button>').on('click', function() {
        if ($('#DropDownSort').text() != value) {
            location.replace('./' + key);
        }
    });
    if ($('#DropDownSort').text() == value) {
        menuItem.addClass('active');
        sort = key.match(/sort=(\w+)/)[1];
        order = key.match(/order=(\w+)/)[1];
    }
    $('#DropDownMenu').append(menuItem);
    //console.log('key:' + key + ' value:' + value);
 }

// ファイルリスト読み込み

let queryTree = {
    path: decodeURI(location.pathname),
    sort: sort,
    order: order,
};

$.getJSON("/filetreeAPI", queryTree)
.done(function(json){
    $('#LoadingIcon').hide();
    if (json.Status != 'ok') {
       $('#ErrorAlart').text(json.Message);
       $('#ErrorAlart').show();
        return;
    }

    let FileList = json.FileList;
    for (let i = 0; i < json.FileList.length; ++i) {
        let item = json.FileList[i];
        //console.log(item);
        let tdIcon = $('<td>');
        let anchorURL = "";
        if (item.isFolder) {
            tdIcon.append('<i class="fa fa-folder  fa-fw" aria-hidden="true"></i>');
            anchorURL = location.pathname + item.name + '/';
        } else {
            tdIcon.append('<i class="fa fa-film  fa-fw" aria-hidden="true"></i>');
            anchorURL = "/play" + location.pathname.substr(5) + item.name;
        }
        anchorURL = anchorURL.replace(/#/g, "<hash>") + location.search;
        let nameAnchor = $('<a>', {
            href: anchorURL
        }).text(item.name);

        let tdName = $('<td>').append(nameAnchor);
        if (!item.isFolder) {
            tdName.append('<span class="float-right">' + item.FileSize + '</span>');
        }
        let tr = $('<tr>').append(tdIcon).append(tdName);
        $('#FileListTable').append(tr);
    }
    //console.log(json);
})
.fail(function(jqXHR, textStatus, errorThrown) {
    console.error("getJSON fail: " + textStatus);
});
