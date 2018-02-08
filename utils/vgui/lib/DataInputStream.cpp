//========= Copyright ?1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include<VGUI_DataInputStream.h>

using namespace vgui;

DataInputStream::DataInputStream(InputStream* is)
{
	_is=is;
}

void DataInputStream::seekStart(bool& success)
{
	if(_is==null)
	{
		success=false;
		return;
	}

	_is->seekStart(success);
}

void DataInputStream::seekRelative(int count,bool& success)
{
	if(_is==null)
	{
		success=false;
		return;
	}

	_is->seekRelative(count,success);
}

void DataInputStream::seekEnd(bool& success)
{
	if(_is==null)
	{
		success=false;
		return;
	}

	_is->seekEnd(success);
}

int DataInputStream::getAvailable(bool& success)
{
	if(_is==null)
	{
		success=false;
		return 0;
	}

	return _is->getAvailable(success);
}

void DataInputStream::readUChar(uchar* buf,int count,bool& success)
{
	if(_is==null)
	{
		success=false;
		return;
	}

	_is->readUChar(buf,count,success);
}

void DataInputStream::close(bool& success)
{
	if(_is==null)
	{
		success=false;
		return;
	}

	_is->close(success);
}

void DataInputStream::close()
{
	bool success;
	_is->close(success);
}

bool DataInputStream::readBool(bool& success)
{
	if(_is==null)
	{
		success=false;
		return false;
	}

	return _is->readUChar(success)!=0;
}

char DataInputStream::readChar(bool& success)
{
	if(_is==null)
	{
		success=false;
		return 0;
	}

	return _is->readUChar(success);
}

uchar DataInputStream::readUChar(bool& success)
{
	if(_is==null)
	{
		success=false;
		return 0;
	}

	return _is->readUChar(success);
}

short DataInputStream::readShort(bool& success)
{
	if(_is==null)
	{
		success=false;
		return 0;
	}

	short ret;
	_is->readUChar((uchar*)&ret,sizeof(ret),success);
	return ret;
}

ushort DataInputStream::readUShort(bool& success)
{
	if(_is==null)
	{
		success=false;
		return 0;
	}

	ushort ret;
	_is->readUChar((uchar*)&ret,sizeof(ret),success);
	return ret;
}

int DataInputStream::readInt(bool& success)
{
	if(_is==null)
	{
		success=false;
		return 0;
	}

	int ret;
	_is->readUChar((uchar*)&ret,sizeof(ret),success);
	return ret;
}

uint DataInputStream::readUInt(bool& success)
{
	if(_is==null)
	{
		success=false;
		return 0;
	}

	uint ret;
	_is->readUChar((uchar*)&ret,sizeof(ret),success);
	return ret;
}

long DataInputStream::readLong(bool& success)
{
	if(_is==null)
	{
		success=false;
		return 0;
	}

	long ret;
	_is->readUChar((uchar*)&ret,sizeof(ret),success);
	return ret;
}

ulong DataInputStream::readULong(bool& success)
{
	if(_is==null)
	{
		success=false;
		return 0;
	}

	ulong ret;
	_is->readUChar((uchar*)&ret,sizeof(ret),success);
	return ret;
}

float DataInputStream::readFloat(bool& success)
{
	if(_is==null)
	{
		success=false;
		return 0;
	}

	float ret;
	_is->readUChar((uchar*)&ret,sizeof(ret),success);
	return ret;
}

double DataInputStream::readDouble(bool& success)
{
	if(_is==null)
	{
		success=false;
		return 0;
	}

	double ret;
	_is->readUChar((uchar*)&ret,sizeof(ret),success);
	return ret;
}

void DataInputStream::readLine(char* buf,int bufLen,bool& success)
{
	if(_is==null)
	{
		success=false;
		return;
	}

	_is->readUChar((uchar*)buf,bufLen,success);
}
