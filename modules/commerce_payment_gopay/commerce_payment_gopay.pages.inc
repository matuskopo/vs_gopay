<?php

function commerce_payment_gopay_rest_notify() {
    watchdog("gopay", "notify: GET: {$_GET['id']}");

    $result = _gopay_validate_and_process_response(false);
    watchdog("gopay", "notify: RESULT for {$_GET['id']}: {$result['message']}");

    return 'OK';
}

function commerce_payment_gopay_return() {
    watchdog("gopay", "return: GET: {$_GET['id']}");

    $result = _gopay_validate_and_process_response();
    watchdog("gopay", "return: RESULT for {$_GET['id']}: {$result['message']}");

    return ['#markup' => $result['message']];
}

function _gopay_validate_and_process_response(Bool $can_redirect = true) {
    if(!isset($_GET['id']) || empty($_GET['id'])) {
        drupal_set_message(t('Payment ID is wrong.'), 'error');
        if($can_redirect) drupal_exit('cart');
    }

    $payment_id = $_GET['id'];
    $rule = rules_config_load("commerce_payment_commerce_payment_gopay");
    $payment_method_instance_id = commerce_payment_method_instance_id("commerce_payment_gopay", $rule);
    $payment_method = commerce_payment_method_instance_load($payment_method_instance_id) ;
    $gopay = commerce_payment_gopay_configure_api($payment_method['settings']);

    if(empty($gopay)) {
        drupal_set_message(t('GoPay not configured correctly.'), 'error');
        if($can_redirect) drupal_exit('cart');
    }

    $response = $gopay->getStatus($payment_id);
    if(empty($response) || !$response->hasSucceed()) {
        drupal_set_message(t('GoPay payment verification failed.'), 'error');
        if($can_redirect) drupal_exit('cart');
    }

    $result = _gopay_parse_response($response);
    if ($result['paid']) {
        $order = commerce_order_load($response->json['order_number']);
        if (!in_array($order->status, ['cart', 'checkout_login', 'checkout_checkout', 'checkout_shipping', 'checkout_review', 'checkout_payment'])) {
            drupal_set_message(t('GoPay payment was already processed. Please, contact support.'), 'error');

            return $result;
        }
        commerce_payment_gopay_transaction($payment_method, $order, ['amount' => $response->json['amount'], 'currency_code' => $response->json['currency'], 'transaction_id' => $response->json['id']]);
        
        if (empty($order->placed)) {
            $order->placed = REQUEST_TIME;
        }
        commerce_order_status_update($order, 'pending', FALSE, NULL, t('Customer payed using GoPay payment.'));
        commerce_checkout_complete($order);
        
        if($can_redirect) drupal_goto(commerce_checkout_order_uri($order));
    } else {
        $order = commerce_order_load($response->json['order_number']);
        if ($order) {
            // Send them back to the shopping cart page instead of the previous page in
            // the checkout process.
            commerce_order_status_update($order, 'checkout_checkout', FALSE, NULL, t('Customer canceled GoPay payment.'));
            drupal_set_message(t('GoPay payment was cancelled. Please, continue to pay again.'), 'error');
            if($can_redirect) drupal_goto('checkout/' . $order->order_id);
        }
    }

    return $result;
}

function _gopay_parse_response(\GoPay\Http\Response $response) {
    $data = $response->json;
    
    $message = "";
    $paid = FALSE;
    
    switch ($data['state']) {
        case 'CREATED':
            $message = "Please, continue with payment here: <a href=\"{$data['gw_url']}\">Proceed to GoPay</a>";
            break;
        case 'PAYMENT_METHOD_CHOSEN':
            $message = "Please, continue with payment here: <a href=\"{$data['gw_url']}\">Proceed to GoPay</a>";
            break;
        case 'PAID':
            $paid = TRUE;
            $message = "Thank you, your purchase completed successfully: <a href=\"/\">Home</a>";
            break;
        case 'AUTHORIZED':
            $message = "Please, continue with payment here: <a href=\"{$data['gw_url']}\">Proceed to GoPay</a>";
            break;
        case 'CANCELED':
            $message = "Payment was cancelled, please contact support: <a href=\"/\">Home</a>";
            break;
        case 'TIMEOUTED':
            $message = "Payment timeout. Please, continue with payment here: <a href=\"{$data['gw_url']}\">Proceed to GoPay</a>";
            break;
        case 'REFUNDED':
            $message = "Don't know what now.";
            break;
        case 'PARTIALLY_REFUNDED':
            $message = "Don't know what now.";
            break;
        
        default:
            $message = "No status.";
            break;
    }

    return [
        'paid' => $paid,
        'message' => $message
    ];
}