package com.uvpainter.io

import android.content.ContentValues
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Environment
import android.provider.MediaStore
import android.provider.OpenableColumns
import android.util.Log
import java.nio.ByteBuffer

/**
 * Entrada y salida de imagenes y modelos.
 *
 * La decodificacion y codificacion PNG se hacen con las APIs de Android en vez
 * de con una libreria nativa: menos peso en el APK y menos superficie de fallo.
 */
object TextureIo {

    private const val TAG = "UVPainterIO"

    data class UriPayload(val bytes: ByteArray?, val extension: String, val displayName: String)

    fun readUri(context: Context, uri: Uri): UriPayload {
        val name = displayName(context, uri)
        val extension = name.substringAfterLast('.', "").lowercase()
        val bytes = runCatching {
            context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
        }.onFailure { Log.e(TAG, "No se pudo leer $uri", it) }.getOrNull()
        return UriPayload(bytes, extension, name)
    }

    private fun displayName(context: Context, uri: Uri): String {
        context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (index >= 0 && cursor.moveToFirst()) return cursor.getString(index)
        }
        return uri.lastPathSegment ?: "modelo"
    }

    /** Guarda RGBA crudo como PNG en Descargas/UVPainter. Devuelve el nombre. */
    fun savePng(context: Context, rgba: ByteArray, size: Int, fileName: String): String? {
        if (size <= 0 || rgba.size < size * size * 4) return null

        val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        // ARGB_8888 guarda los bytes en orden R,G,B,A en memoria, que es
        // justo lo que devuelve glReadPixels con GL_RGBA.
        bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(rgba))

        return runCatching {
            val values = ContentValues().apply {
                put(MediaStore.MediaColumns.DISPLAY_NAME, fileName)
                put(MediaStore.MediaColumns.MIME_TYPE, "image/png")
                put(
                    MediaStore.MediaColumns.RELATIVE_PATH,
                    Environment.DIRECTORY_DOWNLOADS + "/UVPainter",
                )
            }
            val resolver = context.contentResolver
            val uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
                ?: return@runCatching null
            resolver.openOutputStream(uri)?.use { out ->
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, out)
            }
            fileName
        }.onFailure { Log.e(TAG, "Fallo guardando PNG", it) }
            .also { bitmap.recycle() }
            .getOrNull()
    }

    /** Decodifica una imagen y la reescala al tamano del documento. */
    fun readImageAsRgba(context: Context, uri: Uri, size: Int): ByteArray? {
        if (size <= 0) return null
        val decoded = runCatching {
            context.contentResolver.openInputStream(uri)?.use { BitmapFactory.decodeStream(it) }
        }.onFailure { Log.e(TAG, "Fallo decodificando $uri", it) }.getOrNull() ?: return null

        val scaled = if (decoded.width == size && decoded.height == size) {
            decoded
        } else {
            Bitmap.createScaledBitmap(decoded, size, size, true).also {
                if (it !== decoded) decoded.recycle()
            }
        }

        val argb = if (scaled.config == Bitmap.Config.ARGB_8888) {
            scaled
        } else {
            scaled.copy(Bitmap.Config.ARGB_8888, false).also {
                if (it !== scaled) scaled.recycle()
            }
        }

        val buffer = ByteBuffer.allocate(size * size * 4)
        argb.copyPixelsToBuffer(buffer)
        argb.recycle()
        return buffer.array()
    }
}
