#include <Windows.h>

#define ANCHOR_LEFT 1
#define ANCHOR_RIGHT 2
#define ANCHOR_TOP 4
#define ANCHOR_BOTTOM 8

struct CONTROL_ANCHOR{
	int ctrl_id;
	int anchor_mask;
	RECT rect_ctrl,rect_parent;
	int initialized;
};

int AnchorInit(HWND hparent,struct CONTROL_ANCHOR *clist,int clist_len);
int AnchorResize(HWND hparent,struct CONTROL_ANCHOR *clist,int clist_len);

struct WIN_REL_POS{
	WINDOWPLACEMENT parent,win;
	int initialized;
};

int SaveWinRelPosition(HWND hparent,HWND hwin,struct WIN_REL_POS *relpos);
int RestoreWinRelPosition(HWND hparent,HWND hwin,struct WIN_REL_POS *relpos);

int SnapWindow(HWND hwnd,RECT *rect);
int SnapSizing(HWND hwnd,RECT *rect,int side);

int ClampMinWindowSize(RECT *default_size,int side,RECT *srect);

int clamp_min_rect(RECT *rect,int min_w,int min_h);
int clamp_max_rect(RECT *rect,int max_w,int max_h);
int clamp_nearest_screen(RECT *rect);