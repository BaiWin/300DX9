#include "hero_xiaomeiyan.h"

bool b_xiaomeiyan = false;

double xiaomeiyan_WSkill_JiQiang_CD;
double xiaomeiyan_WSkill_Start;
WORD xiaomeiyan_WSkill_ID = 3710;

bool HasJiQiang()
{
	if (xiaomeiyan_WSkill_ID == 3710 && xiaomeiyan_WSkill_JiQiang_CD <= 0)
	{
		return true;
	}
	return false;
}

bool HasPaoDan()
{
	if (xiaomeiyan_WSkill_ID != 3710)
	{
		return true;
	}
	return false;
}