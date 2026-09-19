package com.modrasoba.smartbox

import okhttp3.Call
import okhttp3.Callback
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import org.json.JSONObject
import java.io.IOException
import java.util.concurrent.TimeUnit

/**
 * Thin client for the SMART BOX REST API (see src/web/WebServer.h for the
 * authoritative contract — this mirrors it 1:1, same endpoints the web
 * dashboard uses per spec section 64: "Android aplikacija mora uporabljati
 * isti lokalni/REST API kot web interface"). Session auth uses the same
 * "sb_session" cookie the web UI gets; OkHttp's default CookieJar is a
 * no-op, so this class keeps the cookie itself rather than pulling in a
 * full CookieJar implementation for a single session cookie.
 */
class SmartBoxApi(private var host: String) {
    private val client = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(8, TimeUnit.SECONDS)
        .build()
    private var sessionCookie: String? = null

    fun setHost(newHost: String) { host = newHost }

    private fun baseUrl() = "http://$host"

    private fun request(method: String, path: String, jsonBody: JSONObject?, cb: (Result<JSONObject>) -> Unit) {
        val builder = Request.Builder().url(baseUrl() + path)
        sessionCookie?.let { builder.addHeader("Cookie", it) }
        if (method != "GET") builder.addHeader("X-SmartBox-Request", "1")
        if (jsonBody != null) {
            builder.method(method, jsonBody.toString().toRequestBody("application/json".toMediaType()))
        } else if (method != "GET") {
            builder.method(method, "{}".toRequestBody("application/json".toMediaType()))
        }
        client.newCall(builder.build()).enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) { cb(Result.failure(e)) }
            override fun onResponse(call: Call, response: Response) {
                response.use {
                    val setCookie = it.header("Set-Cookie")
                    if (setCookie != null) sessionCookie = setCookie.substringBefore(";")
                    val bodyStr = it.body?.string() ?: "{}"
                    try {
                        val json = if (bodyStr.isBlank()) JSONObject() else JSONObject(bodyStr)
                        if (!it.isSuccessful) cb(Result.failure(Exception(json.optString("error", "HTTP ${it.code}"))))
                        else cb(Result.success(json))
                    } catch (e: Exception) {
                        cb(Result.failure(e))
                    }
                }
            }
        })
    }

    fun status(cb: (Result<JSONObject>) -> Unit) = request("GET", "/api/status", null, cb)
    fun unlock(cb: (Result<JSONObject>) -> Unit) = request("POST", "/api/lock/unlock", null, cb)
    fun lock(cb: (Result<JSONObject>) -> Unit) = request("POST", "/api/lock/lock", null, cb)
    fun login(username: String, password: String, cb: (Result<JSONObject>) -> Unit) {
        val body = JSONObject().put("username", username).put("password", password)
        request("POST", "/api/auth/login", body, cb)
    }
    fun securityCodeLogin(code: String, cb: (Result<JSONObject>) -> Unit) {
        request("POST", "/api/auth/security-code", JSONObject().put("code", code), cb)
    }
    fun events(limit: Int, cb: (Result<JSONObject>) -> Unit) = request("GET", "/api/events?limit=$limit", null, cb)
    fun users(cb: (Result<JSONObject>) -> Unit) = request("GET", "/api/users", null, cb)
    fun cards(cb: (Result<JSONObject>) -> Unit) = request("GET", "/api/rfid/cards", null, cb)
    fun diagnostics(cb: (Result<JSONObject>) -> Unit) = request("GET", "/api/diagnostics", null, cb)
    // NOTIFICATIONS (spec 64) needs a push channel the ESP32 doesn't have
    // today (no APNs/FCM relay) — see docs/README.md "Future" section;
    // left out of this client rather than stubbed with a fake endpoint.
}
