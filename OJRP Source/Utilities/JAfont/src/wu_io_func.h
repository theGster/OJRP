//wu_io_func.h

typedef struct {
	float num;
} float_t;

typedef struct {
	short num;
} short_t;

float getf( FILE *file );
int gets( FILE *file );
void putf( float number, FILE *file );
void puts( int number, FILE *file );
void StringSet( char *in, const char *msg, ... );
