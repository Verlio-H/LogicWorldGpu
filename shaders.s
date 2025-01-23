@org 0x0 //vertex shader																											//shader 0
ILOD R1 R0 0																														025010000
ILOD R2 R0 2																														025020003
DLOD R2 R2 3 0 //contains VBO index																									030020214
BSL R3 R1 1																															006030101
ADD R2 R2 R3																														000020203
DLOD R4 R2 0 0																														030040200
DLOD R5 R2 1 0																														030050204
DSTR R4 R3 0 1																														031040301
DSTR R5 R3 1 1																														031050305
HLT																																	047000000
NOP																																	000000000

@org 0x100 //input assembly/geometry stage/rasterization init																		//shader 1
ILOD R1 R0 0																														025010000
ILOD R2 R0 2																														025020002
DLOD R2 R2 3 0 //contains EBO index																									030020214
BSL R3 R1 2																															006030102
ADD R2 R2 R3																														000020203
DLOD R4 R2 0 0 //p0 index																											030040200
DLOD R5 R2 1 0 //p1 index																											030050204
DLOD R6 R2 2 0 //p2 index																											030060210
IMM R9 1																															017110001
AADD R9 R0 R9																														034110011
BSL R3 R9 3																															006031103
BSL R10 R9 2																														006121102
ADD R3 R3 R10																														000030312
//find bounding box																													
DLOD R11 R4 0 1 //x0																												030130401
DLOD R12 R5 0 1 //x1																												030140501
DLOD R13 R6 0 1 //x2																												030150601
MOV R45 R11																															000551300
FSUB R30 R11 R13 //x0-x2																											011361315
FSUB R31 R12 R11 //x1-x0																											011371413
FSUB R32 R13 R11 //x2-x0																											011401513


IMM R35 0x540																														017432500
BSL R35 R35 4 //32.0f																												006434304
FDIV R33 R31 R35 //potentially swap to FMLT, but 16f can't be reused																013413743
FDIV R34 R32 R35																													013424043
FDIV R35 R35 R35																													013434343

PSHM //swap order if necessary																										043000000
SUB R0 R12 R11																														001001413
FLGM N //if (R12 < R11)																												046000004
	MOV R14 R12																														000161400
	MOV R12 R11																														000141300
	MOV R11 R14																														000131600
POPM																																044000000

PSHM																																043000000
SUB R0 R13 R11																														001001513
FLGM N //if (R13 < R11)																												046000004
	MOV R11 R13																														000131500
XORM //else																															045000000
	SUB R0 R13 R12																													001001514
	FLGM NN //if (R13 >= R12)																										046000005
		MOV R12 R13																													000141500
POPM																																044000000																					
IMM R13 0x3C0 //0.5f																												017151700
BSL R13 R13 4																														006151504
FADD R12 R12 R13																													010141415
//NOP
//NOP
DLOD R21 R4 1 1 //y0																												030250405
DLOD R22 R5 1 1 //y1																												030260505
DLOD R23 R6 1 1 //y2																												030270605
MOV R46 R21																															000562500
FSUB R42 R23 R21																													011522725
FSUB R40 R21 R22																													011502526
FSUB R41 R22 R21																													011512625
//
FTOI R11 R11																														015131300
FMLT R33 R33 R42																													012414152
ITOF R49 R11																														016611300
FSUB R45 R49 R45																													011556155
FTOI R12 R12																														015141400
FMLT R34 R41 R34																													012425142
SUB R12 R12 R11																														001141413
FSUB R33 R33 R34 //determinant																										011414142
ADDI R12 R12 1																														004141401
BSR R12 R12 1																														007141401
BSL R12 R12 2 //x size * 2, R11 = x start																							006141402
FDIV R33 R35 R33																													013414341

PSHM //swap order if necessary																										043000000
SUB R0 R22 R21																														001002625
FLGM N //if (R22 < R21)																												046000004
	MOV R14 R22																														000162600
	MOV R22 R21																														000262500
	MOV R21 R14																														000251600
POPM																																044000000

FMLT R42 R42 R33 //uincx																											012525241
FMLT R40 R40 R33 //vincx																											012505041
FMLT R30 R30 R33 //uincy																											012363641
FMLT R31 R31 R33 //vincy																											012373741
FMLT R47 R45 R42																													012575552

PSHM																																043000000
SUB R0 R23 R21																														001272500
FLGM N //if (R23 < R21)																												046000004
	MOV R21 R23																														000252700
XORM //else																															045000000
	SUB R0 R23 R22																													001002726
	FLGM NN //if (R23 >= R22)																										046000005
		MOV R22 R23																													000262700
POPM																																044000000
//todo: calculate rasterization increment values

FADD R22 R22 R13																													010262615
FTOI R21 R21																														015252500
FMLT R48 R45 R40																													012605550
ITOF R49 R21																														016612500
IMM R1 0x200																														017011000
FSUB R46 R49 R46																													011566156
DSTR R1 R3 2 0																														031010310
FTOI R22 R22																														015262600
SUB R22 R22 R21																														001262625
FMLT R49 R46 R30																													012615636
ADDI R22 R22 1																														004262601
FADD R47 R47 R49 //u value																											010575761
BSR R22 R22 1 //y size/2, R21 = y start																								007262601
//x size is intentionally doubled and y size is intentionally halved to construct quads in fragment shader
FMLT R49 R46 R31																													012615637
DSTR R12 R3 0 0																														031140300
FADD R48 R48 R49 //v value																											010606061
DSTR R22 R3 1 0																														031260304
DSTR R11 R3 3 0	//x start																											031130314
DSTR R21 R3 4 0	//y start																											031250320
DSTR R42 R3 5 0 //ux																												031520324
DSTR R40 R3 6 0 //vx																												031500330
DSTR R30 R3 7 0 //uy																												031360334
DSTR R31 R3 8 0 //vy																												031370340
DSTR R47 R3 9 0 //u start																											031570344
DSTR R48 R3 10 0 //v start																											031600350
HLT																																	047000000
NOP																																	000000000

@org 0x200 //fragment shading																										//shader 2
ILOD R1 R0 0 //thread id x																											025010000
ILOD R2 R0 1 //thread id y																											025020001
ILOD R3 R0 2 //command ptr																											025030002
DLOD R4 R3 3 0 //x start																											030040314
DLOD R5 R3 4 0 //y start																											030050320
DLOD R10 R3 5 0 //ux																												030120324
DLOD R11 R3 6 0 //vx																												030130330
DLOD R12 R3 7 0 //uy																												030140334
DLOD R13 R3 8 0 //vy																												030150340
DLOD R14 R3 9 0 //u start																											030160344
DLOD R15 R3 10 0 //v start																											030170350
BSL R2 R2 1																															006020201
IMM R6 1																															017060001
AND R6 R1 R6																														002060106
ADD R2 R2 R6																														000020206
BSR R1 R1 1																															007010101
ITOF R16 R1 //x float																												016200100
ITOF R17 R2 //y float																												016210200
FMLT R10 R10 R16																													012121220
FMLT R11 R11 R16																													012131320
FMLT R12 R12 R17																													012141421
FMLT R13 R13 R17																													012151521
ADD R63 R5 R2 //ycoord																												000770502
FADD R10 R10 R12																													010121214
IMM R2 0x400																														017022000
FADD R11 R11 R13																													010131315
IMM R24 0x540																														017302500
FADD R10 R10 R14 //u																												010121216
ADD R62 R4 R1 //xcoord																												000760401
PSHM																																043000000
FADD R11 R11 R15 //v																												010131317
BSL R24 R24 4 //32.0f																												006303004
FDIV R10 R10 R24																													013121230
MOV R10 R10																															000121200
FLGM NN																																046000005
	FDIV R11 R11 R24																												013131330
	BSL R2 R2 4 //1.0f																												006020204
	FSUB R2 R2 R10																													011020212
	MOV R11 R11																														000131300
	FLGM NN																															046000005
		IMM R1 0x5FF																												017012777
		FSUB R2 R2 R11																												011020213
		BSL R1 R1 4																													006010104
		ADDI R1 R1 8																												004010110
		FMLT R10 R10 R1																												012121201
		MOV R2 R2																													000020200
		FLGM NN																														046000005
			FMLT R11 R11 R1																											012131301
			FMLT R2 R2 R1																											012020201
			FTOI R10 R10																											015121200
			FTOI R11 R11																											015131300
			FTOI R2 R2																												015020200
			OUT R10 R11 R2																											032121302
POPM																																044000000
HLT																																	047000000
NOP																																	000000000