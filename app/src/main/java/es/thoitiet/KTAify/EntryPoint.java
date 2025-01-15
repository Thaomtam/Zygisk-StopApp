package es.thoitiet.KTAify;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;

import org.json.JSONObject;
import org.lsposed.hiddenapibypass.HiddenApiBypass;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.security.KeyStore;
import java.security.KeyStoreSpi;
import java.security.Provider;
import java.security.Security;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

public class EntryPoint {
    private static final String TAG = "KTAify";

    /**
     * Phương thức init được gọi từ JNI.
     *
     * @param jsonConfig       JSON cấu hình từ C++.
     * @param spoofProvider    Cờ quyết định spoof Provider.
     * @param spoofSignature   Cờ quyết định spoof Signature.
     */
    public static void init(String jsonConfig, boolean spoofProvider, boolean spoofSignature) {
        Log.d(TAG, "EntryPoint.init called");
        Log.d(TAG, "JSON Config: " + jsonConfig);
        Log.d(TAG, "Spoof Provider: " + spoofProvider);
        Log.d(TAG, "Spoof Signature: " + spoofSignature);

        try {
            // Parse JSON
            JSONObject config = new JSONObject(jsonConfig);

            // Kiểm tra DEBUG flag
            boolean isDebug = config.optBoolean("DEBUG", false);
            if (isDebug) {
                Log.d(TAG, "DEBUG mode is enabled for this app");
            }

            // Spoof Build properties if requested
            if (config.optBoolean("spoofProps", false)) {
                spoofBuildProperties(config);
            }

            // Spoof provider logic if requested
            if (spoofProvider) {
                handleSpoofProvider(config);
            }

            // Spoof signature logic if requested
            if (spoofSignature) {
                handleSpoofSignature(config);
            }

            Log.d(TAG, "EntryPoint.init completed successfully");
        } catch (Exception e) {
            Log.e(TAG, "Error during EntryPoint.init", e);
        }
    }

    /**
     * Spoof các trường trong android.os.Build và Build.VERSION.
     *
     * @param config JSON cấu hình chứa các trường cần spoof.
     */
    private static void spoofBuildProperties(JSONObject config) {
        try {
            Log.d(TAG, "Spoofing Build properties");

            setBuildField("MANUFACTURER", config.optString("MANUFACTURER", Build.MANUFACTURER));
            setBuildField("MODEL", config.optString("MODEL", Build.MODEL));
            setBuildField("BRAND", config.optString("BRAND", Build.BRAND));
            setBuildField("PRODUCT", config.optString("PRODUCT", Build.PRODUCT));
            setBuildField("DEVICE", config.optString("DEVICE", Build.DEVICE));
            setBuildField("FINGERPRINT", config.optString("FINGERPRINT", Build.FINGERPRINT));
            setBuildField("TYPE", config.optString("TYPE", Build.TYPE));
            setBuildField("TAGS", config.optString("TAGS", Build.TAGS));

            // Update Build.VERSION fields
            setBuildVersionField("RELEASE", config.optString("RELEASE", Build.VERSION.RELEASE));
            setBuildVersionField("ID", config.optString("ID", Build.ID));
            setBuildVersionField("INCREMENTAL", config.optString("INCREMENTAL", Build.VERSION.INCREMENTAL));
            setBuildVersionField("SECURITY_PATCH", config.optString("SECURITY_PATCH", Build.VERSION.SECURITY_PATCH));
        } catch (Exception e) {
            Log.e(TAG, "Error spoofing Build properties", e);
        }
    }

    /**
     * Set field cho android.os.Build thông qua Reflection.
     *
     * @param fieldName Tên trường cần thay đổi.
     * @param value     Giá trị mới.
     */
    private static void setBuildField(String fieldName, String value) {
        try {
            java.lang.reflect.Field field = Build.class.getDeclaredField(fieldName);
            field.setAccessible(true);
            field.set(null, value);
            Log.d(TAG, "Set Build." + fieldName + " to " + value);
        } catch (Exception e) {
            Log.e(TAG, "Error setting Build." + fieldName, e);
        }
    }

    /**
     * Set field cho android.os.Build.VERSION thông qua Reflection.
     *
     * @param fieldName Tên trường cần thay đổi.
     * @param value     Giá trị mới.
     */
    private static void setBuildVersionField(String fieldName, String value) {
        try {
            java.lang.reflect.Field field = Build.VERSION.class.getDeclaredField(fieldName);
            field.setAccessible(true);
            field.set(null, value);
            Log.d(TAG, "Set Build.VERSION." + fieldName + " to " + value);
        } catch (Exception e) {
            Log.e(TAG, "Error setting Build.VERSION." + fieldName, e);
        }
    }

    /**
     * Xử lý spoof Provider logic.
     *
     * @param config JSON cấu hình.
     */
    private static void handleSpoofProvider(JSONObject config) {
        try {
            Log.d(TAG, "Handling spoof Provider logic");
            // Logic cụ thể cho việc spoof Provider
            if (config.has("providerName")) {
                String providerName = config.getString("providerName");
                Log.d(TAG, "Spoofing provider to: " + providerName);
                // Thực hiện spoof Provider (nếu cần)
            }
        } catch (Exception e) {
            Log.e(TAG, "Error handling spoof Provider", e);
        }
    }

    /**
     * Xử lý spoof Signature logic.
     *
     * @param config JSON cấu hình.
     */
    private static void handleSpoofSignature(JSONObject config) {
        try {
            Log.d(TAG, "Handling spoof Signature logic");
            // Logic cụ thể cho việc spoof Signature
            if (config.has("signatureKey")) {
                String signatureKey = config.getString("signatureKey");
                Log.d(TAG, "Spoofing signature to: " + signatureKey);
                // Thực hiện spoof Signature (nếu cần)
            }
        } catch (Exception e) {
            Log.e(TAG, "Error handling spoof Signature", e);
        }
    }
}