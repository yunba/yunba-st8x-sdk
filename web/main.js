var APPKEY = '563c4afef085fc471efdf803';
var TOPIC_REPORT = 'yunba_lock_report';
var ALIAS = 'yunba_lock_test';


$(document).ready(function() {
  window.send_time = null;
  $('#span-status').text('正在连接云巴服务器...');

  window.yunba = new Yunba({
    server: 'sock.yunba.io',
    port: 3000,
    appkey: APPKEY
  });

  // 初始化云巴 SDK
  yunba.init(function(success) {
    if (success) {
      var cid = Math.random().toString().substr(2);
      console.log('cid: ' + cid);
      window.alias = cid;

      // 连接云巴服务器
      yunba.connect_by_customid(cid,
        function(success, msg, sessionid) {
          if (success) {
            console.log('sessionid：' + sessionid);

            // 设置收到信息回调函数
            yunba.set_message_cb(yunba_msg_cb);
              // TOPIC
              yunba.subscribe({
                  'topic': TOPIC_REPORT
                },
                function(success, msg) {
                  if (success) {
                    console.log('subscribed');
                    yunba_sub_ok();
                  } else {
                    console.log(msg);
                  }
                }
              );
          } else {
            console.log(msg);
          }
        });
    } else {
      console.log('yunba init failed');
    }
  });
});

$('#btn-send').click(function() {
  yunba.publish_to_alias({
    'alias': ALIAS,
    'msg': 'unlock',
    }, function (success, msg) {
        if (!success) {
            console.log(msg);
        }
    });
  window.send_time = new Date();
});


function yunba_msg_cb(data) {
  console.log(data);
  if (data.topic === TOPIC_REPORT) {
    // 在线信息
    var msg = JSON.parse(data.msg);
    if (msg.lock == true) {
      $('#span-status').text('状态: 已锁上');
      $('#btn-send').attr("disabled", false);
    } else {
      if (window.send_time != null) {
      	var recv_time = new Date();
      	var sec = (recv_time.getTime() - window.send_time.getTime()) / 1000.0;
      	$('#span-status').text('状态: 已打开(用时 ' + sec + ' 秒)');
		window.send_time = null;
      } else {
        $('#span-status').text('状态: 已打开');
      }
      $('#btn-send').attr("disabled", true);
    }
    $('#span-loading').css("display", "none");
  }
}

function yunba_sub_ok() {
  $('#span-status').text('正在获取锁的状态...');
  yunba.publish_to_alias({
    'alias': ALIAS,
    'msg': 'report',
    }, function (success, msg) {
        if (!success) {
            console.log(msg);
        }
    });
}
