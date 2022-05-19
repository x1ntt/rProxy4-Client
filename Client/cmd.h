#ifndef _CMD_H
#define _CMD_H

#include "buffer.h"

#define MAKE_HEAD Make_head(buffer,tmp.size(),0);

namespace CMD {
	enum CMDID {
		CMD_ERROR = 0,		// ����ֵ ����
		CMD_LOGIN,				// ��¼
		CMD_LOGIN_RE,			// ��¼��Ӧ
		CMD_SESSION_END,			// �Ự����
	};

	struct login {
		CmdId cmdid;
		Cid cid;
		us16 note_size;
		char note[];
	};

	Buffer& Make_head(Buffer& buffer, Sid sid);

	Buffer& Make_data(Buffer& buffer, Buffer& data, Sid sid);

	// ���¶��ǳ������� ����Ҫ����sid ��Ϊsid����0
	Buffer& Make_login(Buffer& buffer, Cid cid, std::string& note);

	// cid == 0, ��ʾ��¼ʧ�� cid��ռ����
	Buffer& Make_login_re(Buffer& buffer, Cid cid);
	Buffer& Make_session_end(Buffer& buffer, Sid sid);
};

#endif