#include <stdio.h>
#include <bson.h>
#include <assert.h>

// gcc -o writer writer.c -I/usr/local/include/libbson-1.0 -lbson-1.0

int main (int argc, char *argv[])
{
   bson_writer_t *writer;
   bson_t *doc;
   uint8_t *buf = NULL;
   size_t buflen = 0;
   bool r;
   int i;

   writer = bson_writer_new (&buf, &buflen, 0, bson_realloc_ctx, NULL);

   //for (i = 0; i < 10000; i++) {
      r = bson_writer_begin (writer, &doc);
      assert (r);

      r = BSON_APPEND_UTF8 (doc, "hello", "world");
      BSON_APPEND_UTF8 (doc, "lua", "bson");
      assert (r);

      bson_writer_end (writer);
   //}

   FILE *f = fopen( "test.bson","wb" );
   if ( !f )
   {
      printf( "open file fail!" );
      return 2;
   }

   int w = fwrite( buf,1,buflen,f );
   printf("%d byte write to file",w);
   fclose( f );

   bson_writer_destroy( writer );
   bson_free (buf);

   return 0;
}
