package com.cloudinary;

import java.util.*;

import com.cloudinary.api.ApiResponse;
import com.cloudinary.api.AuthorizationRequired;
import com.cloudinary.api.exceptions.*;
import com.cloudinary.metadata.MetadataField;
import com.cloudinary.metadata.MetadataDataSource;
import com.cloudinary.strategies.AbstractApiStrategy;
import com.cloudinary.utils.ObjectUtils;
import com.cloudinary.utils.StringUtils;
import org.cloudinary.json.JSONArray;

@SuppressWarnings({"rawtypes", "unchecked"})
public class Api {


    public AbstractApiStrategy getStrategy() {
        return strategy;
    }

    public enum HttpMethod {GET, POST, PUT, DELETE;}

    public final static Map<Integer, Class<? extends Exception>> CLOUDINARY_API_ERROR_CLASSES = new HashMap<Integer, Class<? extends Exception>>();

    static {
        CLOUDINARY_API_ERROR_CLASSES.put(400, BadRequest.class);
        CLOUDINARY_API_ERROR_CLASSES.put(401, AuthorizationRequired.class);
        CLOUDINARY_API_ERROR_CLASSES.put(403, NotAllowed.class);
        CLOUDINARY_API_ERROR_CLASSES.put(404, NotFound.class);
        CLOUDINARY_API_ERROR_CLASSES.put(409, AlreadyExists.class);
        CLOUDINARY_API_ERROR_CLASSES.put(420, RateLimited.class);
        CLOUDINARY_API_ERROR_CLASSES.put(500, GeneralError.class);
    }

    public final Cloudinary cloudinary;

    private AbstractApiStrategy strategy;

    protected ApiResponse callApi(HttpMethod method, Iterable<String> uri, Map<String, ? extends Object> params, Map options) throws Exception {
        return this.strategy.callApi(method, uri, params, options);
    }

    public Api(Cloudinary cloudinary, AbstractApiStrategy strategy) {
        this.cloudinary = cloudinary;
        this.strategy = strategy;
        this.strategy.init(this);
    }

    public ApiResponse ping(Map options) throws Exception {
        if (options == null) options = ObjectUtils.emptyMap();
        return callApi(HttpMethod.GET, Arrays.asList("ping"), ObjectUtils.emptyMap(), options);
    }

    public ApiResponse usage(Map options) throws Exception {
        if (options == null) options = ObjectUtils.emptyMap();

        final List<String> uri = new ArrayList<String>();
        uri.add("usage");

        Object date = options.get("date");

        if (date != null) {
            if (date instanceof Date) {
                date = ObjectUtils.toUsageApiDateFormat((Date) date);
            }

            uri.add(date.toString());
        }

        return callApi(HttpMethod.GET, uri, ObjectUtils.emptyMap(), options);
    }