/*  
**********************************************************************
*   Copyright (C) 2002-2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   file name:  ucnv_u16.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2002jul01
*   created by: Markus W. Scherer
*
*   UTF-16 converter implementation. Used to be in ucnv_utf.c.
*/

#include "unicode/utypes.h"
#include "unicode/ucnv.h"
#include "unicode/ucnv_err.h"
#include "ucnv_bld.h"
#include "ucnv_cnv.h"
#include "cmemory.h"

/* UTF-16BE ----------------------------------------------------------------- */

#if U_IS_BIG_ENDIAN
#   define _UTF16PEFromUnicodeWithOffsets   _UTF16BEFromUnicodeWithOffsets
#else
#   define _UTF16PEFromUnicodeWithOffsets   _UTF16LEFromUnicodeWithOffsets
#endif

static void
_UTF16BEFromUnicodeWithOffsets(UConverterFromUnicodeArgs *pArgs,
                               UErrorCode *pErrorCode) {
    UConverter *cnv;
    const UChar *source;
    uint8_t *target;
    int32_t *offsets;

    int32_t targetCapacity, length, count, sourceIndex;
    UChar c, trail;
    char overflow[4];

    source=pArgs->source;
    length=pArgs->sourceLimit-source;
    if(length<=0) {
        /* no input, nothing to do */
        return;
    }

    targetCapacity=pArgs->targetLimit-pArgs->target;
    if(targetCapacity<=0) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
        return;
    }

    cnv=pArgs->converter;
    target=(uint8_t *)pArgs->target;
    offsets=pArgs->offsets;
    sourceIndex=0;

    /* c!=0 indicates in several places outside the main loops that a surrogate was found */

    if((c=(UChar)cnv->fromUChar32)!=0 && U16_IS_TRAIL(trail=*source) && targetCapacity>=4) {
        /* the last buffer ended with a lead surrogate, output the surrogate pair */
        ++source;
        --length;
        target[0]=(uint8_t)(c>>8);
        target[1]=(uint8_t)c;
        target[2]=(uint8_t)(trail>>8);
        target[3]=(uint8_t)trail;
        target+=4;
        targetCapacity-=4;
        if(offsets!=NULL) {
            *offsets++=-1;
            *offsets++=-1;
            *offsets++=-1;
            *offsets++=-1;
        }
        sourceIndex=1;
        cnv->fromUChar32=c=0;
    }

    /* copy an even number of bytes for complete UChars */
    count=2*length;
    if(count>targetCapacity) {
        count=targetCapacity&~1;
    }
    /* count is even */
    if(c==0) {
        targetCapacity-=count;
        count>>=1;
        length-=count;

        if(offsets==NULL) {
            while(count>0) {
                c=*source++;
                if(U16_IS_SINGLE(c)) {
                    target[0]=(uint8_t)(c>>8);
                    target[1]=(uint8_t)c;
                    target+=2;
                } else if(U16_IS_SURROGATE_LEAD(c) && count>=2 && U16_IS_TRAIL(trail=*source)) {
                    ++source;
                    --count;
                    target[0]=(uint8_t)(c>>8);
                    target[1]=(uint8_t)c;
                    target[2]=(uint8_t)(trail>>8);
                    target[3]=(uint8_t)trail;
                    target+=4;
                } else {
                    break;
                }
                --count;
            }
        } else {
            while(count>0) {
                c=*source++;
                if(U16_IS_SINGLE(c)) {
                    target[0]=(uint8_t)(c>>8);
                    target[1]=(uint8_t)c;
                    target+=2;
                    *offsets++=sourceIndex;
                    *offsets++=sourceIndex++;
                } else if(U16_IS_SURROGATE_LEAD(c) && count>=2 && U16_IS_TRAIL(trail=*source)) {
                    ++source;
                    --count;
                    target[0]=(uint8_t)(c>>8);
                    target[1]=(uint8_t)c;
                    target[2]=(uint8_t)(trail>>8);
                    target[3]=(uint8_t)trail;
                    target+=4;
                    *offsets++=sourceIndex;
                    *offsets++=sourceIndex;
                    *offsets++=sourceIndex;
                    *offsets++=sourceIndex;
                    sourceIndex+=2;
                } else {
                    break;
                }
                --count;
            }
        }

        if(count==0) {
            /* done with the loop for complete UChars */
            if(length>0 && targetCapacity>0) {
                /*
                 * there is more input and some target capacity -
                 * it must be targetCapacity==1 because otherwise
                 * the above would have copied more;
                 * prepare for overflow output
                 */
                if(U16_IS_SINGLE(c=*source++)) {
                    overflow[0]=(char)(c>>8);
                    overflow[1]=(char)c;
                    length=2; /* 2 bytes to output */
                    c=0;
                /* } else { keep c for surrogate handling, length will be set there */
                }
            } else {
                length=0;
                c=0;
            }
        } else {
            /* keep c for surrogate handling, length will be set there */
            targetCapacity+=2*count;
        }
    } else {
        length=0; /* from here on, length counts the bytes in overflow[] */
    }
    
    if(c!=0) {
        /*
         * c is a surrogate, and
         * - source or target too short
         * - or the surrogate is unmatched
         */
        length=0;
        if(U16_IS_SURROGATE_LEAD(c)) {
            if(source<pArgs->sourceLimit) {
                if(U16_IS_TRAIL(trail=*source)) {
                    /* output the surrogate pair, will overflow (see conditions comment above) */
                    ++source;
                    overflow[0]=(char)(c>>8);
                    overflow[1]=(char)c;
                    overflow[2]=(char)(trail>>8);
                    overflow[3]=(char)trail;
                    length=4; /* 4 bytes to output */
                    c=0;
                } else {
                    /* unmatched lead surrogate */
                    *pErrorCode=U_ILLEGAL_CHAR_FOUND;
                }
            } else {
                /* see if the trail surrogate is in the next buffer */
            }
        } else {
            /* unmatched trail surrogate */
            *pErrorCode=U_ILLEGAL_CHAR_FOUND;
        }
        cnv->fromUChar32=c;
    }

    if(length>0) {
        /* output length bytes with overflow (length>targetCapacity>0) */
        ucnv_fromUWriteBytes(cnv,
                             overflow, length,
                             (char **)&target, pArgs->targetLimit,
                             &offsets, sourceIndex,
                             pErrorCode);
        targetCapacity=pArgs->targetLimit-(char *)target;
    }

    if(U_SUCCESS(*pErrorCode) && source<pArgs->sourceLimit && targetCapacity==0) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
    }

    /* write back the updated pointers */
    pArgs->source=source;
    pArgs->target=(char *)target;
    pArgs->offsets=offsets;
}

static void
_UTF16BEToUnicodeWithOffsets(UConverterToUnicodeArgs *pArgs,
                             UErrorCode *pErrorCode) {
    UConverter *cnv;
    const uint8_t *source;
    UChar *target;
    int32_t *offsets;

    int32_t targetCapacity, length, count, sourceIndex;
    UChar c, trail;

    cnv=pArgs->converter;
    source=(const uint8_t *)pArgs->source;
    length=(const uint8_t *)pArgs->sourceLimit-source;
    if(length<=0 && cnv->toUnicodeStatus==0) {
        /* no input, nothing to do */
        return;
    }

    targetCapacity=pArgs->targetLimit-pArgs->target;
    if(targetCapacity<=0) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
        return;
    }

    target=pArgs->target;
    offsets=pArgs->offsets;
    sourceIndex=0;
    c=0;

    /* complete a partial UChar or pair from the last call */
    if(cnv->toUnicodeStatus!=0) {
        /*
         * special case: single byte from a previous buffer,
         * where the byte turned out not to belong to a trail surrogate
         * and the preceding, unmatched lead surrogate was put into toUBytes[]
         * for error handling
         */
        cnv->toUBytes[0]=(uint8_t)cnv->toUnicodeStatus;
        cnv->toULength=1;
        cnv->toUnicodeStatus=0;
    }
    if((count=cnv->toULength)!=0) {
        uint8_t *p=cnv->toUBytes;
        do {
            p[count++]=*source++;
            ++sourceIndex;
            --length;
            if(count==2) {
                c=((UChar)p[0]<<8)|p[1];
                if(U16_IS_SINGLE(c)) {
                    /* output the BMP code point */
                    *target++=c;
                    if(offsets!=NULL) {
                        *offsets++=-1;
                    }
                    --targetCapacity;
                    count=0;
                    c=0;
                    break;
                } else if(U16_IS_SURROGATE_LEAD(c)) {
                    /* continue collecting bytes for the trail surrogate */
                    c=0; /* avoid unnecessary surrogate handling below */
                } else {
                    /* fall through to error handling for an unmatched trail surrogate */
                    break;
                }
            } else if(count==4) {
                c=((UChar)p[0]<<8)|p[1];
                trail=((UChar)p[2]<<8)|p[3];
                if(U16_IS_TRAIL(trail)) {
                    /* output the surrogate pair */
                    *target++=c;
                    if(targetCapacity>=2) {
                        *target++=trail;
                        if(offsets!=NULL) {
                            *offsets++=-1;
                            *offsets++=-1;
                        }
                        targetCapacity-=2;
                    } else /* targetCapacity==1 */ {
                        targetCapacity=0;
                        cnv->UCharErrorBuffer[0]=trail;
                        cnv->UCharErrorBufferLength=1;
                        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
                    }
                    count=0;
                    c=0;
                    break;
                } else {
                    /* unmatched lead surrogate, handle here for consistent toUBytes[] */
                    *pErrorCode=U_ILLEGAL_CHAR_FOUND;

                    /* back out reading the code unit after it */
                    if(((const uint8_t *)pArgs->source-source)>=2) {
                        source-=2;
                    } else {
                        /*
                         * if the trail unit's first byte was in a previous buffer, then
                         * we need to put it into a special place because toUBytes[] will be
                         * used for the lead unit's bytes
                         */
                        cnv->toUnicodeStatus=0x100|p[2];
                        --source;
                    }
                    cnv->toULength=2;

                    /* write back the updated pointers */
                    pArgs->source=(const char *)source;
                    pArgs->target=target;
                    pArgs->offsets=offsets;
                    return;
                }
            }
        } while(length>0);
        cnv->toULength=(int8_t)count;
    }

    /* copy an even number of bytes for complete UChars */
    count=2*targetCapacity;
    if(count>length) {
        count=length&~1;
    }
    if(c==0 && count>0) {
        length-=count;
        count>>=1;
        targetCapacity-=count;
        if(offsets==NULL) {
            do {
                c=((UChar)source[0]<<8)|source[1];
                source+=2;
                if(U16_IS_SINGLE(c)) {
                    *target++=c;
                } else if(U16_IS_SURROGATE_LEAD(c) && count>=2 &&
                          U16_IS_TRAIL(trail=((UChar)source[0]<<8)|source[1])
                ) {
                    source+=2;
                    --count;
                    *target++=c;
                    *target++=trail;
                } else {
                    break;
                }
            } while(--count>0);
        } else {
            do {
                c=((UChar)source[0]<<8)|source[1];
                source+=2;
                if(U16_IS_SINGLE(c)) {
                    *target++=c;
                    *offsets++=sourceIndex;
                    sourceIndex+=2;
                } else if(U16_IS_SURROGATE_LEAD(c) && count>=2 &&
                          U16_IS_TRAIL(trail=((UChar)source[0]<<8)|source[1])
                ) {
                    source+=2;
                    --count;
                    *target++=c;
                    *target++=trail;
                    *offsets++=sourceIndex;
                    *offsets++=sourceIndex;
                    sourceIndex+=4;
                } else {
                    break;
                }
            } while(--count>0);
        }

        if(count==0) {
            /* done with the loop for complete UChars */
            c=0;
        } else {
            /* keep c for surrogate handling, trail will be set there */
            length+=2*(count-1); /* one more byte pair was consumed than count decremented */
            targetCapacity+=count;
        }
    }

    if(c!=0) {
        /*
         * c is a surrogate, and
         * - source or target too short
         * - or the surrogate is unmatched
         */
        cnv->toUBytes[0]=(uint8_t)(c>>8);
        cnv->toUBytes[1]=(uint8_t)c;
        cnv->toULength=2;

        if(U16_IS_SURROGATE_LEAD(c)) {
            if(length>=2) {
                if(U16_IS_TRAIL(trail=((UChar)source[0]<<8)|source[1])) {
                    /* output the surrogate pair, will overflow (see conditions comment above) */
                    source+=2;
                    length-=2;
                    *target++=c;
                    if(offsets!=NULL) {
                        *offsets++=sourceIndex;
                    }
                    cnv->UCharErrorBuffer[0]=trail;
                    cnv->UCharErrorBufferLength=1;
                    cnv->toULength=0;
                    *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
                } else {
                    /* unmatched lead surrogate */
                    *pErrorCode=U_ILLEGAL_CHAR_FOUND;
                }
            } else {
                /* see if the trail surrogate is in the next buffer */
            }
        } else {
            /* unmatched trail surrogate */
            *pErrorCode=U_ILLEGAL_CHAR_FOUND;
        }
    }

    if(U_SUCCESS(*pErrorCode)) {
        /* check for a remaining source byte */
        if(length>0) {
            if(targetCapacity==0) {
                *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
            } else {
                /* it must be length==1 because otherwise the above would have copied more */
                cnv->toUBytes[cnv->toULength++]=*source++;
            }
        }
    }

    /* write back the updated pointers */
    pArgs->source=(const char *)source;
    pArgs->target=target;
    pArgs->offsets=offsets;
}

static UChar32
_UTF16BEGetNextUChar(UConverterToUnicodeArgs *pArgs, UErrorCode *err) {
    const uint8_t *s, *sourceLimit;
    UChar32 c;

    s=(const uint8_t *)pArgs->source;
    sourceLimit=(const uint8_t *)pArgs->sourceLimit;

    if(s>=sourceLimit) {
        /* no input */
        *err=U_INDEX_OUTOFBOUNDS_ERROR;
        return 0xffff;
    }

    if(s+2>sourceLimit) {
        /* only one byte: truncated UChar */
        pArgs->converter->toUBytes[0]=*s++;
        pArgs->converter->toULength=1;
        pArgs->source=(const char *)s;
        *err = U_TRUNCATED_CHAR_FOUND;
        return 0xffff;
    }

    /* get one UChar */
    c=((UChar32)*s<<8)|s[1];
    s+=2;

    /* check for a surrogate pair */
    if(U_IS_SURROGATE(c)) {
        if(U16_IS_SURROGATE_LEAD(c)) {
            if(s+2<=sourceLimit) {
                UChar trail;

                /* get a second UChar and see if it is a trail surrogate */
                trail=((UChar)*s<<8)|s[1];
                if(U16_IS_TRAIL(trail)) {
                    c=U16_GET_SUPPLEMENTARY(c, trail);
                    s+=2;
                } else {
                    /* unmatched lead surrogate */
                    c=-2;
                }
            } else {
                /* too few (2 or 3) bytes for a surrogate pair: truncated code point */
                uint8_t *bytes=pArgs->converter->toUBytes;
                s-=2;
                pArgs->converter->toULength=(int8_t)(sourceLimit-s);
                do {
                    *bytes++=*s++;
                } while(s<sourceLimit);

                c=0xffff;
                *err=U_TRUNCATED_CHAR_FOUND;
            }
        } else {
            /* unmatched trail surrogate */
            c=-2;
        }

        if(c<0) {
            /* write the unmatched surrogate */
            uint8_t *bytes=pArgs->converter->toUBytes;
            pArgs->converter->toULength=2;
            *bytes=*(s-2);
            bytes[1]=*(s-1);

            c=0xffff;
            *err=U_ILLEGAL_CHAR_FOUND;
        }
    }

    pArgs->source=(const char *)s;
    return c;
} 

static const UConverterImpl _UTF16BEImpl={
    UCNV_UTF16_BigEndian,

    NULL,
    NULL,

    NULL,
    NULL,
    NULL,

    _UTF16BEToUnicodeWithOffsets,
    _UTF16BEToUnicodeWithOffsets,
    _UTF16BEFromUnicodeWithOffsets,
    _UTF16BEFromUnicodeWithOffsets,
    _UTF16BEGetNextUChar,

    NULL,
    NULL,
    NULL,
    NULL,
    ucnv_getCompleteUnicodeSet
};

static const UConverterStaticData _UTF16BEStaticData={
    sizeof(UConverterStaticData),
    "UTF-16BE",
    1200, UCNV_IBM, UCNV_UTF16_BigEndian, 2, 2,
    { 0xff, 0xfd, 0, 0 },2,FALSE,FALSE,
    0,
    0,
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } /* reserved */
};


const UConverterSharedData _UTF16BEData={
    sizeof(UConverterSharedData), ~((uint32_t) 0),
    NULL, NULL, &_UTF16BEStaticData, FALSE, &_UTF16BEImpl, 
    0
};

/* UTF-16LE ----------------------------------------------------------------- */

static void
_UTF16LEFromUnicodeWithOffsets(UConverterFromUnicodeArgs *pArgs,
                               UErrorCode *pErrorCode) {
    UConverter *cnv;
    const UChar *source;
    uint8_t *target;
    int32_t *offsets;

    int32_t targetCapacity, length, count, sourceIndex;
    UChar c, trail;
    char overflow[4];

    source=pArgs->source;
    length=pArgs->sourceLimit-source;
    if(length<=0) {
        /* no input, nothing to do */
        return;
    }

    targetCapacity=pArgs->targetLimit-pArgs->target;
    if(targetCapacity<=0) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
        return;
    }

    cnv=pArgs->converter;
    target=(uint8_t *)pArgs->target;
    offsets=pArgs->offsets;
    sourceIndex=0;

    /* c!=0 indicates in several places outside the main loops that a surrogate was found */

    if((c=(UChar)cnv->fromUChar32)!=0 && U16_IS_TRAIL(trail=*source) && targetCapacity>=4) {
        /* the last buffer ended with a lead surrogate, output the surrogate pair */
        ++source;
        --length;
        target[0]=(uint8_t)c;
        target[1]=(uint8_t)(c>>8);
        target[2]=(uint8_t)trail;
        target[3]=(uint8_t)(trail>>8);
        target+=4;
        targetCapacity-=4;
        if(offsets!=NULL) {
            *offsets++=-1;
            *offsets++=-1;
            *offsets++=-1;
            *offsets++=-1;
        }
        sourceIndex=1;
        cnv->fromUChar32=c=0;
    }

    /* copy an even number of bytes for complete UChars */
    count=2*length;
    if(count>targetCapacity) {
        count=targetCapacity&~1;
    }
    /* count is even */
    if(c==0) {
        targetCapacity-=count;
        count>>=1;
        length-=count;

        if(offsets==NULL) {
            while(count>0) {
                c=*source++;
                if(U16_IS_SINGLE(c)) {
                    target[0]=(uint8_t)c;
                    target[1]=(uint8_t)(c>>8);
                    target+=2;
                } else if(U16_IS_SURROGATE_LEAD(c) && count>=2 && U16_IS_TRAIL(trail=*source)) {
                    ++source;
                    --count;
                    target[0]=(uint8_t)c;
                    target[1]=(uint8_t)(c>>8);
                    target[2]=(uint8_t)trail;
                    target[3]=(uint8_t)(trail>>8);
                    target+=4;
                } else {
                    break;
                }
                --count;
            }
        } else {
            while(count>0) {
                c=*source++;
                if(U16_IS_SINGLE(c)) {
                    target[0]=(uint8_t)c;
                    target[1]=(uint8_t)(c>>8);
                    target+=2;
                    *offsets++=sourceIndex;
                    *offsets++=sourceIndex++;
                } else if(U16_IS_SURROGATE_LEAD(c) && count>=2 && U16_IS_TRAIL(trail=*source)) {
                    ++source;
                    --count;
                    target[0]=(uint8_t)c;
                    target[1]=(uint8_t)(c>>8);
                    target[2]=(uint8_t)trail;
                    target[3]=(uint8_t)(trail>>8);
                    target+=4;
                    *offsets++=sourceIndex;
                    *offsets++=sourceIndex;
                    *offsets++=sourceIndex;
                    *offsets++=sourceIndex;
                    sourceIndex+=2;
                } else {
                    break;
                }
                --count;
            }
        }

        if(count==0) {
            /* done with the loop for complete UChars */
            if(length>0 && targetCapacity>0) {
                /*
                 * there is more input and some target capacity -
                 * it must be targetCapacity==1 because otherwise
                 * the above would have copied more;
                 * prepare for overflow output
                 */
                if(U16_IS_SINGLE(c=*source++)) {
                    overflow[0]=(char)c;
                    overflow[1]=(char)(c>>8);
                    length=2; /* 2 bytes to output */
                    c=0;
                /* } else { keep c for surrogate handling, length will be set there */
                }
            } else {
                length=0;
                c=0;
            }
        } else {
            /* keep c for surrogate handling, length will be set there */
            targetCapacity+=2*count;
        }
    } else {
        length=0; /* from here on, length counts the bytes in overflow[] */
    }
    
    if(c!=0) {
        /*
         * c is a surrogate, and
         * - source or target too short
         * - or the surrogate is unmatched
         */
        length=0;
        if(U16_IS_SURROGATE_LEAD(c)) {
            if(source<pArgs->sourceLimit) {
                if(U16_IS_TRAIL(trail=*source)) {
                    /* output the surrogate pair, will overflow (see conditions comment above) */
                    ++source;
                    overflow[0]=(char)c;
                    overflow[1]=(char)(c>>8);
                    overflow[2]=(char)trail;
                    overflow[3]=(char)(trail>>8);
                    length=4; /* 4 bytes to output */
                    c=0;
                } else {
                    /* unmatched lead surrogate */
                    *pErrorCode=U_ILLEGAL_CHAR_FOUND;
                }
            } else {
                /* see if the trail surrogate is in the next buffer */
            }
        } else {
            /* unmatched trail surrogate */
            *pErrorCode=U_ILLEGAL_CHAR_FOUND;
        }
        cnv->fromUChar32=c;
    }

    if(length>0) {
        /* output length bytes with overflow (length>targetCapacity>0) */
        ucnv_fromUWriteBytes(cnv,
                             overflow, length,
                             (char **)&target, pArgs->targetLimit,
                             &offsets, sourceIndex,
                             pErrorCode);
        targetCapacity=pArgs->targetLimit-(char *)target;
    }

    if(U_SUCCESS(*pErrorCode) && source<pArgs->sourceLimit && targetCapacity==0) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
    }

    /* write back the updated pointers */
    pArgs->source=source;
    pArgs->target=(char *)target;
    pArgs->offsets=offsets;
}

static void
_UTF16LEToUnicodeWithOffsets(UConverterToUnicodeArgs *pArgs,
                             UErrorCode *pErrorCode) {
    UConverter *cnv;
    const uint8_t *source;
    UChar *target;
    int32_t *offsets;

    int32_t targetCapacity, length, count, sourceIndex;
    UChar c, trail;

    cnv=pArgs->converter;
    source=(const uint8_t *)pArgs->source;
    length=(const uint8_t *)pArgs->sourceLimit-source;
    if(length<=0 && cnv->toUnicodeStatus==0) {
        /* no input, nothing to do */
        return;
    }

    targetCapacity=pArgs->targetLimit-pArgs->target;
    if(targetCapacity<=0) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
        return;
    }

    target=pArgs->target;
    offsets=pArgs->offsets;
    sourceIndex=0;
    c=0;

    /* complete a partial UChar or pair from the last call */
    if(cnv->toUnicodeStatus!=0) {
        /*
         * special case: single byte from a previous buffer,
         * where the byte turned out not to belong to a trail surrogate
         * and the preceding, unmatched lead surrogate was put into toUBytes[]
         * for error handling
         */
        cnv->toUBytes[0]=(uint8_t)cnv->toUnicodeStatus;
        cnv->toULength=1;
        cnv->toUnicodeStatus=0;
    }
    if((count=cnv->toULength)!=0) {
        uint8_t *p=cnv->toUBytes;
        do {
            p[count++]=*source++;
            ++sourceIndex;
            --length;
            if(count==2) {
                c=((UChar)p[1]<<8)|p[0];
                if(U16_IS_SINGLE(c)) {
                    /* output the BMP code point */
                    *target++=c;
                    if(offsets!=NULL) {
                        *offsets++=-1;
                    }
                    --targetCapacity;
                    count=0;
                    c=0;
                    break;
                } else if(U16_IS_SURROGATE_LEAD(c)) {
                    /* continue collecting bytes for the trail surrogate */
                    c=0; /* avoid unnecessary surrogate handling below */
                } else {
                    /* fall through to error handling for an unmatched trail surrogate */
                    break;
                }
            } else if(count==4) {
                c=((UChar)p[1]<<8)|p[0];
                trail=((UChar)p[3]<<8)|p[2];
                if(U16_IS_TRAIL(trail)) {
                    /* output the surrogate pair */
                    *target++=c;
                    if(targetCapacity>=2) {
                        *target++=trail;
                        if(offsets!=NULL) {
                            *offsets++=-1;
                            *offsets++=-1;
                        }
                        targetCapacity-=2;
                    } else /* targetCapacity==1 */ {
                        targetCapacity=0;
                        cnv->UCharErrorBuffer[0]=trail;
                        cnv->UCharErrorBufferLength=1;
                        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
                    }
                    count=0;
                    c=0;
                    break;
                } else {
                    /* unmatched lead surrogate, handle here for consistent toUBytes[] */
                    *pErrorCode=U_ILLEGAL_CHAR_FOUND;

                    /* back out reading the code unit after it */
                    if(((const uint8_t *)pArgs->source-source)>=2) {
                        source-=2;
                    } else {
                        /*
                         * if the trail unit's first byte was in a previous buffer, then
                         * we need to put it into a special place because toUBytes[] will be
                         * used for the lead unit's bytes
                         */
                        cnv->toUnicodeStatus=0x100|p[2];
                        --source;
                    }
                    cnv->toULength=2;

                    /* write back the updated pointers */
                    pArgs->source=(const char *)source;
                    pArgs->target=target;
                    pArgs->offsets=offsets;
                    return;
                }
            }
        } while(length>0);
        cnv->toULength=(int8_t)count;
    }

    /* copy an even number of bytes for complete UChars */
    count=2*targetCapacity;
    if(count>length) {
        count=length&~1;
    }
    if(c==0 && count>0) {
        length-=count;
        count>>=1;
        targetCapacity-=count;
        if(offsets==NULL) {
            do {
                c=((UChar)source[1]<<8)|source[0];
                source+=2;
                if(U16_IS_SINGLE(c)) {
                    *target++=c;
                } else if(U16_IS_SURROGATE_LEAD(c) && count>=2 &&
                          U16_IS_TRAIL(trail=((UChar)source[1]<<8)|source[0])
                ) {
                    source+=2;
                    --count;
                    *target++=c;
                    *target++=trail;
                } else {
                    break;
                }
            } while(--count>0);
        } else {
            do {
                c=((UChar)source[1]<<8)|source[0];
                source+=2;
                if(U16_IS_SINGLE(c)) {
                    *target++=c;
                    *offsets++=sourceIndex;
                    sourceIndex+=2;
                } else if(U16_IS_SURROGATE_LEAD(c) && count>=2 &&
                          U16_IS_TRAIL(trail=((UChar)source[1]<<8)|source[0])
                ) {
                    source+=2;
                    --count;
                    *target++=c;
                    *target++=trail;
                    *offsets++=sourceIndex;
                    *offsets++=sourceIndex;
                    sourceIndex+=4;
                } else {
                    break;
                }
            } while(--count>0);
        }

        if(count==0) {
            /* done with the loop for complete UChars */
            c=0;
        } else {
            /* keep c for surrogate handling, trail will be set there */
            length+=2*(count-1); /* one more byte pair was consumed than count decremented */
            targetCapacity+=count;
        }
    }

    if(c!=0) {
        /*
         * c is a surrogate, and
         * - source or target too short
         * - or the surrogate is unmatched
         */
        cnv->toUBytes[0]=(uint8_t)c;
        cnv->toUBytes[1]=(uint8_t)(c>>8);
        cnv->toULength=2;

        if(U16_IS_SURROGATE_LEAD(c)) {
            if(length>=2) {
                if(U16_IS_TRAIL(trail=((UChar)source[1]<<8)|source[0])) {
                    /* output the surrogate pair, will overflow (see conditions comment above) */
                    source+=2;
                    length-=2;
                    *target++=c;
                    if(offsets!=NULL) {
                        *offsets++=sourceIndex;
                    }
                    cnv->UCharErrorBuffer[0]=trail;
                    cnv->UCharErrorBufferLength=1;
                    cnv->toULength=0;
                    *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
                } else {
                    /* unmatched lead surrogate */
                    *pErrorCode=U_ILLEGAL_CHAR_FOUND;
                }
            } else {
                /* see if the trail surrogate is in the next buffer */
            }
        } else {
            /* unmatched trail surrogate */
            *pErrorCode=U_ILLEGAL_CHAR_FOUND;
        }
    }

    if(U_SUCCESS(*pErrorCode)) {
        /* check for a remaining source byte */
        if(length>0) {
            if(targetCapacity==0) {
                *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
            } else {
                /* it must be length==1 because otherwise the above would have copied more */
                cnv->toUBytes[cnv->toULength++]=*source++;
            }
        }
    }

    /* write back the updated pointers */
    pArgs->source=(const char *)source;
    pArgs->target=target;
    pArgs->offsets=offsets;
}

static UChar32
_UTF16LEGetNextUChar(UConverterToUnicodeArgs *pArgs, UErrorCode *err) {
    const uint8_t *s, *sourceLimit;
    UChar32 c;

    s=(const uint8_t *)pArgs->source;
    sourceLimit=(const uint8_t *)pArgs->sourceLimit;

    if(s>=sourceLimit) {
        /* no input */
        *err=U_INDEX_OUTOFBOUNDS_ERROR;
        return 0xffff;
    }

    if(s+2>sourceLimit) {
        /* only one byte: truncated UChar */
        pArgs->converter->toUBytes[0]=*s++;
        pArgs->converter->toULength=1;
        pArgs->source=(const char *)s;
        *err = U_TRUNCATED_CHAR_FOUND;
        return 0xffff;
    }

    /* get one UChar */
    c=((UChar32)s[1]<<8)|*s;
    s+=2;

    /* check for a surrogate pair */
    if(U_IS_SURROGATE(c)) {
        if(U16_IS_SURROGATE_LEAD(c)) {
            if(s+2<=sourceLimit) {
                UChar trail;

                /* get a second UChar and see if it is a trail surrogate */
                trail=((UChar)s[1]<<8)|*s;
                if(U16_IS_TRAIL(trail)) {
                    c=U16_GET_SUPPLEMENTARY(c, trail);
                    s+=2;
                } else {
                    /* unmatched lead surrogate */
                    c=-2;
                }
            } else {
                /* too few (2 or 3) bytes for a surrogate pair: truncated code point */
                uint8_t *bytes=pArgs->converter->toUBytes;
                s-=2;
                pArgs->converter->toULength=(int8_t)(sourceLimit-s);
                do {
                    *bytes++=*s++;
                } while(s<sourceLimit);

                c=0xffff;
                *err=U_TRUNCATED_CHAR_FOUND;
            }
        } else {
            /* unmatched trail surrogate */
            c=-2;
        }

        if(c<0) {
            /* write the unmatched surrogate */
            uint8_t *bytes=pArgs->converter->toUBytes;
            pArgs->converter->toULength=2;
            *bytes=*(s-2);
            bytes[1]=*(s-1);

            c=0xffff;
            *err=U_ILLEGAL_CHAR_FOUND;
        }
    }

    pArgs->source=(const char *)s;
    return c;
} 

static const UConverterImpl _UTF16LEImpl={
    UCNV_UTF16_LittleEndian,

    NULL,
    NULL,

    NULL,
    NULL,
    NULL,

    _UTF16LEToUnicodeWithOffsets,
    _UTF16LEToUnicodeWithOffsets,
    _UTF16LEFromUnicodeWithOffsets,
    _UTF16LEFromUnicodeWithOffsets,
    _UTF16LEGetNextUChar,

    NULL,
    NULL,
    NULL,
    NULL,
    ucnv_getCompleteUnicodeSet
};


static const UConverterStaticData _UTF16LEStaticData={
    sizeof(UConverterStaticData),
    "UTF-16LE",
    1202, UCNV_IBM, UCNV_UTF16_LittleEndian, 2, 2,
    { 0xfd, 0xff, 0, 0 },2,FALSE,FALSE,
    0,
    0,
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } /* reserved */
};


const UConverterSharedData _UTF16LEData={
    sizeof(UConverterSharedData), ~((uint32_t) 0),
    NULL, NULL, &_UTF16LEStaticData, FALSE, &_UTF16LEImpl, 
    0
};

/* UTF-16 (Detect BOM) ------------------------------------------------------ */

/*
 * Detect a BOM at the beginning of the stream and select UTF-16BE or UTF-16LE
 * accordingly.
 * This is a simpler version of the UTF-32 converter below, with
 * fewer states for shorter BOMs.
 *
 * State values:
 * 0    initial state
 * 1    saw FE
 * 2..4 -
 * 5    saw FF
 * 6..7 -
 * 8    UTF-16BE mode
 * 9    UTF-16LE mode
 *
 * During detection: state&3==number of matching bytes so far.
 *
 * On output, emit U+FEFF as the first code point.
 */

static void
_UTF16Reset(UConverter *cnv, UConverterResetChoice choice) {
    if(choice<=UCNV_RESET_TO_UNICODE) {
        /* reset toUnicode: state=0 */
        cnv->mode=0;
    }
    if(choice!=UCNV_RESET_TO_UNICODE) {
        /* reset fromUnicode: prepare to output the UTF-16PE BOM */
        cnv->charErrorBufferLength=2;
#if U_IS_BIG_ENDIAN
        cnv->charErrorBuffer[0]=0xfe;
        cnv->charErrorBuffer[1]=0xff;
#else
        cnv->charErrorBuffer[0]=0xff;
        cnv->charErrorBuffer[1]=0xfe;
#endif
    }
}

static void
_UTF16Open(UConverter *cnv,
           const char *name,
           const char *locale,
           uint32_t options,
           UErrorCode *pErrorCode) {
    _UTF16Reset(cnv, UCNV_RESET_BOTH);
}

static const char utf16BOM[8]={ (char)0xfe, (char)0xff, 0, 0,    (char)0xff, (char)0xfe, 0, 0 };

static void
_UTF16ToUnicodeWithOffsets(UConverterToUnicodeArgs *pArgs,
                           UErrorCode *pErrorCode) {
    UConverter *cnv=pArgs->converter;
    const char *source=pArgs->source;
    const char *sourceLimit=pArgs->sourceLimit;
    int32_t *offsets=pArgs->offsets;

    int32_t state, offsetDelta;
    char b;

    state=cnv->mode;

    /*
     * If we detect a BOM in this buffer, then we must add the BOM size to the
     * offsets because the actual converter function will not see and count the BOM.
     * offsetDelta will have the number of the BOM bytes that are in the current buffer.
     */
    offsetDelta=0;

    while(source<sourceLimit && U_SUCCESS(*pErrorCode)) {
        switch(state) {
        case 0:
            b=*source;
            if(b==(char)0xfe) {
                state=1; /* could be FE FF */
            } else if(b==(char)0xff) {
                state=5; /* could be FF FE */
            } else {
                state=8; /* default to UTF-16BE */
                continue;
            }
            ++source;
            break;
        case 1:
        case 5:
            if(*source==utf16BOM[state]) {
                ++source;
                if(state==1) {
                    state=8; /* detect UTF-16BE */
                    offsetDelta=source-pArgs->source;
                } else if(state==5) {
                    state=9; /* detect UTF-16LE */
                    offsetDelta=source-pArgs->source;
                }
            } else {
                /* switch to UTF-16BE and pass the previous bytes */
                if(source!=pArgs->source) {
                    /* just reset the source */
                    source=pArgs->source;
                } else {
                    UBool oldFlush=pArgs->flush;

                    /* the first byte is from a previous buffer, replay it first */
                    pArgs->source=utf16BOM+(state&4); /* select the correct BOM */
                    pArgs->sourceLimit=pArgs->source+1; /* replay previous byte */
                    pArgs->flush=FALSE; /* this sourceLimit is not the real source stream limit */

                    _UTF16BEToUnicodeWithOffsets(pArgs, pErrorCode);

                    /* restore real pointers; pArgs->source will be set in case 8/9 */
                    pArgs->sourceLimit=sourceLimit;
                    pArgs->flush=oldFlush;
                }
                state=8;
                continue;
            }
            break;
        case 8:
            /* call UTF-16BE */
            pArgs->source=source;
            _UTF16BEToUnicodeWithOffsets(pArgs, pErrorCode);
            source=pArgs->source;
            break;
        case 9:
            /* call UTF-16LE */
            pArgs->source=source;
            _UTF16LEToUnicodeWithOffsets(pArgs, pErrorCode);
            source=pArgs->source;
            break;
        default:
            break; /* does not occur */
        }
    }

    /* add BOM size to offsets - see comment at offsetDelta declaration */
    if(offsets!=NULL && offsetDelta!=0) {
        int32_t *offsetsLimit=pArgs->offsets;
        while(offsets<offsetsLimit) {
            *offsets++ += offsetDelta;
        }
    }

    pArgs->source=source;

    if(source==sourceLimit && pArgs->flush) {
        /* handle truncated input */
        switch(state) {
        case 0:
            break; /* no input at all, nothing to do */
        case 8:
            _UTF16BEToUnicodeWithOffsets(pArgs, pErrorCode);
            break;
        case 9:
            _UTF16LEToUnicodeWithOffsets(pArgs, pErrorCode);
            break;
        default:
            /* handle 0<state<8: call UTF-16BE with too-short input */
            pArgs->source=utf16BOM+(state&4); /* select the correct BOM */
            pArgs->sourceLimit=pArgs->source+(state&3); /* replay bytes */

            /* no offsets: not enough for output */
            _UTF16BEToUnicodeWithOffsets(pArgs, pErrorCode);
            pArgs->source=source;
            pArgs->sourceLimit=sourceLimit;
            state=8;
            break;
        }
    }

    cnv->mode=state;
}

static UChar32
_UTF16GetNextUChar(UConverterToUnicodeArgs *pArgs,
                   UErrorCode *pErrorCode) {
    switch(pArgs->converter->mode) {
    case 8:
        return _UTF16BEGetNextUChar(pArgs, pErrorCode);
    case 9:
        return _UTF16LEGetNextUChar(pArgs, pErrorCode);
    default:
        return UCNV_GET_NEXT_UCHAR_USE_TO_U;
    }
}

static const UConverterImpl _UTF16Impl = {
    UCNV_UTF16,

    NULL,
    NULL,

    _UTF16Open,
    NULL,
    _UTF16Reset,

    _UTF16ToUnicodeWithOffsets,
    _UTF16ToUnicodeWithOffsets,
    _UTF16PEFromUnicodeWithOffsets,
    _UTF16PEFromUnicodeWithOffsets,
    _UTF16GetNextUChar,

    NULL, /* ### TODO implement getStarters for all Unicode encodings?! */
    NULL,
    NULL,
    NULL,
    ucnv_getCompleteUnicodeSet
};

static const UConverterStaticData _UTF16StaticData = {
    sizeof(UConverterStaticData),
    "UTF-16",
    0, /* ### TODO review correctness of all Unicode CCSIDs */
    UCNV_IBM, UCNV_UTF16, 2, 2,
#if U_IS_BIG_ENDIAN
    { 0xff, 0xfd, 0, 0 }, 2,
#else
    { 0xfd, 0xff, 0, 0 }, 2,
#endif
    FALSE, FALSE,
    0,
    0,
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } /* reserved */
};

const UConverterSharedData _UTF16Data = {
    sizeof(UConverterSharedData), ~((uint32_t) 0),
    NULL, NULL, &_UTF16StaticData, FALSE, &_UTF16Impl, 
    0
};
