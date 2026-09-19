package com.modrasoba.smartbox

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout

/**
 * Minimal status/unlock/lock screen (spec section 64's "status / unlock /
 * lock" functions). Users / cards / events / diagnostics / settings screens
 * are intentionally not built out here yet — SmartBoxApi.kt already has
 * the calls ready (users(), cards(), events(), diagnostics()); wiring them
 * to real fragments/screens is the natural next step once the core
 * status/unlock/lock loop is validated against real hardware. "Open full
 * dashboard" opens the same web UI the desktop browser uses, so nothing is
 * ever missing from the app even before those screens exist.
 */
class MainActivity : AppCompatActivity() {
    private lateinit var api: SmartBoxApi

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val hostField = findViewById<EditText>(R.id.deviceHost)
        val statusText = findViewById<TextView>(R.id.statusText)
        val refresh = findViewById<SwipeRefreshLayout>(R.id.refresh)

        val prefs = getSharedPreferences("smartbox", MODE_PRIVATE)
        hostField.setText(prefs.getString("host", ""))

        api = SmartBoxApi(hostField.text.toString())

        fun refreshStatus() {
            api.status { result ->
                runOnUiThread {
                    refresh.isRefreshing = false
                    result.onSuccess { json ->
                        statusText.text = "Lock: ${json.optString("lockState")}\n" +
                            "Lid: ${json.optString("lid")}\n" +
                            "WiFi: ${json.optString("wifi")}  IP: ${json.optString("ip")}"
                    }.onFailure {
                        statusText.text = "Not connected: ${it.message}"
                    }
                }
            }
        }

        findViewById<Button>(R.id.connectBtn).setOnClickListener {
            val host = hostField.text.toString().trim()
            prefs.edit().putString("host", host).apply()
            api.setHost(host)
            refreshStatus()
        }

        findViewById<Button>(R.id.unlockBtn).setOnClickListener {
            api.unlock { result ->
                runOnUiThread {
                    result.onSuccess { Toast.makeText(this, "Unlock requested", Toast.LENGTH_SHORT).show(); refreshStatus() }
                        .onFailure { Toast.makeText(this, "Error: ${it.message}", Toast.LENGTH_SHORT).show() }
                }
            }
        }

        findViewById<Button>(R.id.lockBtn).setOnClickListener {
            api.lock { result ->
                runOnUiThread {
                    result.onSuccess { Toast.makeText(this, "Lock requested", Toast.LENGTH_SHORT).show(); refreshStatus() }
                        .onFailure { Toast.makeText(this, "Error: ${it.message}", Toast.LENGTH_SHORT).show() }
                }
            }
        }

        findViewById<Button>(R.id.openWebBtn).setOnClickListener {
            val host = hostField.text.toString().trim()
            if (host.isEmpty()) { Toast.makeText(this, "Enter the device IP first", Toast.LENGTH_SHORT).show(); return@setOnClickListener }
            startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("http://$host/")))
        }

        refresh.setOnRefreshListener { refreshStatus() }
        if (hostField.text.isNotEmpty()) refreshStatus()
    }
}
